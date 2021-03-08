#include "mpi.h"
#include <iostream>
#include <thread>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <queue>
#include <cctype>
#include <algorithm>
#include <condition_variable>
#include <mutex>

#define HORROR 0
#define COMEDY 1
#define FANTASY 2
#define SCIENCE_FICTION 3
#define WORKER_HORROR 1
#define WORKER_COMEDY 2
#define WORKER_FANTASY 3
#define WORKER_SCIENCE_FICTION 4

using namespace std;

// maximul de thread-uri al sistemului
int max_threads = 0;
// ordinea de scriere
queue<int> order;
// verificare O(1) daca o litera e consoana
bool is_consonant[127] = {false};
// unde tin textul procesat inainte sa-l scriu
vector<vector<string> > finishedText;
// ce thread e la rand sa scrie in finishedText
int threadToDump;
// cate thread-uri au terminat de procesat textul
int finishedThreads = 0;
// sincronizare
mutex m;
condition_variable cv;
condition_variable write_barrier;

// comunicarea fiecarui thread cu worker-ul lui
void send_and_recv(int id, vector<string> paragraph, vector<string> &newParagraph) {
    // trimitem nr de linii
    int size = paragraph.size();
    MPI_Send(&size, 1, MPI_INT, id + 1, id + 1, MPI_COMM_WORLD);

    // trimitem linie cu linie
    for (int line = 0; line < size; line++) {
        // id + 1 pt ca thread-urile incep de la 0, iar workerii de la 1
        MPI_Send(paragraph[line].c_str(), paragraph[line].length() + 1, MPI_CHAR, id + 1, id + 1, MPI_COMM_WORLD);
    }

    // primim linie cu linie
    MPI_Status status;
    for (int i = 0; i < size; i++) {
        MPI_Probe(id + 1, id + 1, MPI_COMM_WORLD, &status);
        int line_size;
        // vedem ce dimensiune are linia ca sa alocam dinamic
        MPI_Get_count(&status, MPI_CHAR, &line_size);
        char *line = (char *)calloc(sizeof(char), line_size);
        MPI_Recv(line, line_size, MPI_CHAR, id + 1, id + 1, MPI_COMM_WORLD, &status);
        newParagraph.push_back(string(line));
    }
}

void read_file(int id, string inFilename) {
    string line;
    ifstream in;
    in.open(inFilename.c_str());
    ofstream out;
    string outFilename = inFilename.substr(0, inFilename.find_last_of('.')) + ".out";
    out.open(outFilename.c_str());
    string paragraph_name;
    vector<string> paragraph;
    vector<string> newParagraph;
    bool add = false;
    bool found = true;
    bool empty_line = false;
    int stop_signal = -1;
    unique_lock<mutex> lk(m);

    // setam dupa ce paragrafe ne uitam in fisier
    switch(id) {
        case HORROR:
            paragraph_name = string("horror");
            break;
        case COMEDY:
            paragraph_name = string("comedy");
            break;
        case FANTASY:
            paragraph_name = string("fantasy");
            break;
        case SCIENCE_FICTION:
            paragraph_name = string("science-fiction");
            break;
        }

    // citim linie cu linie
    while (getline(in, line)) {
        // punem intr-un queue ordinea genurilor
        // ca oricum inter-genre se pastreaza ordinea
        if (id == 0) {
            if (line == string("horror")) {
                if (order.empty()) {
                    threadToDump = HORROR;
                }
                order.push(HORROR);
            }

            if (line == string("comedy")) {
                if (order.empty()) {
                    threadToDump = COMEDY;
                }
                order.push(COMEDY);
            }

            if (line == string("fantasy")) {
                if (order.empty()) {
                    threadToDump = FANTASY;
                }
                order.push(FANTASY);
            }

            if (line == string("science-fiction")) {
                if (order.empty()) {
                    threadToDump = SCIENCE_FICTION;
                }
                order.push(SCIENCE_FICTION);
            }

            cv.notify_all();
        }
        
        // setam ca am terminat paragraful curent
        if (line == "" && add) {
            empty_line = true;
        }

        // am terminat paragraful curent si trb sa-l trimitem
        if (empty_line) {
            empty_line = false;
            add = false;

            // basically daca paragraful citit este
            // paragraful de care se intereseaza thread-ul
            if (found) {
                found = false;

                newParagraph.clear();
                newParagraph.push_back(paragraph_name);

                send_and_recv(id, paragraph, newParagraph);

                paragraph.clear();

                // asteptam randul sa scriem in finishedText
                cv.wait(lk, [id]() { return (threadToDump == id); });

                finishedText.push_back(newParagraph);
                order.pop();
                // spunem urmatorului thread sa se trezeasca
                threadToDump = order.front();
                cv.notify_all();
            }
        }

        // daca suntem la paragraful bun, memoram ce citim
        if (add && !empty_line) {
            paragraph.push_back(line);
        }

        // incepem paragraf nou
        if (line == paragraph_name) {
            add = true;
            found = true;
        }
    }

    // bariera improvizata
    finishedThreads++;
    if (finishedThreads == 4) {
        write_barrier.notify_all();
    } else {
        write_barrier.wait(lk);
    }
    
    // daca cumva nu am tratat ultimul paragraf
    // nu e captat in while pt ca nu se termina cu 2 * \n ca restul
    if (paragraph.size() != 0) {
        newParagraph.clear();
        newParagraph.push_back(paragraph_name);

        send_and_recv(id, paragraph, newParagraph);

        finishedText.push_back(newParagraph);
    }

    // spunem workerilor ca au muncit destul
    MPI_Send(&stop_signal, 1, MPI_INT, id + 1, id + 1, MPI_COMM_WORLD);
    // un singur thread scrie in fisier
    if (id == 0) {
        for (auto paragraph : finishedText) {
            for (auto line : paragraph) {
                out << line << "\n";
            }
            out << "\n";
        }
    }

}

// functie de prelucrat paragraful fantasy
void fantasy(vector<string>::iterator begin, vector<string>::iterator end) {
    for (auto it = begin; it != end; it++) {
        bool upper = true;

        for (int pos = 0; (*it)[pos] != '\0'; pos++) {
            if (upper) {
                upper = false;
                (*it)[pos] = toupper((*it)[pos]);
            }

            if (isspace((*it)[pos])) {
                upper = true;
            }
        }
    }
}

// functie de prelucrat paragraful comedy
void comedy(vector<string>::iterator begin, vector<string>::iterator end) {
    for (auto it = begin; it != end; it++) {
        int word_letter_counter = 1;

        for (int pos = 0; (*it)[pos] != '\0'; pos++) {
            if ((*it)[pos] == ' ') {
                word_letter_counter = 0;
            }

            if (word_letter_counter % 2 == 0) {
                (*it)[pos] = toupper((*it)[pos]);
            }

            word_letter_counter++;
        }
    }
}

// functie de prelucrat paragraful horror
void horror(vector<string>::iterator begin, vector<string>::iterator end) {
    for (auto it = begin; it != end; ++it) {
        string newline;

        for (int pos = 0; (*it)[pos] != '\0'; pos++) {
            newline.push_back((*it)[pos]);
            if (is_consonant[(*it)[pos]]) {
                newline.push_back(tolower((*it)[pos]));
            }
        }

        *it = newline;
    }
}

// functie de prelucrat paragraful science-fiction
void sf(vector<string>::iterator begin, vector<string>::iterator end) {
    for (auto it = begin; it != end; it++) {
        int word_counter = 1;
        bool reversed = false;

        for (int pos = 0; (*it)[pos] != '\0'; pos++) {
            if (word_counter % 7 == 0 && !reversed) { // pos e space sigur
                reversed = true;
                int next_space = (*it).find(' ', pos);
                
                // cuvantul e ultimul pe linie
                if (next_space == -1) {
                    next_space = (*it).length();
                }
                
                reverse((*it).begin() + pos, (*it).begin() + next_space);
            }

            if ((*it)[pos] == ' ') {
                word_counter++;
                reversed = false;
            }
        }
    }
}

// initializarea array-ului pentru a verifica in O(1) daca o litera e consoana
void init_consonant_table() {
    for (int i = 'A'; i <= 'z'; i++) {
        if (i > 'Z' && i < 'a') {
            continue;
        }

        char c = tolower(i);

        if (c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u') {
            is_consonant[i] = true;
        }
    }
}

// codul care ruleaza pe thread-ul propriu zis
void worker_thread_function(int rank, vector<string> &paragraph, int start, int end, int iterations) {
    // setam ce functie trb sa aplicam
    void (*processing)(vector<string>::iterator, vector<string>::iterator);
    switch (rank) {
        case WORKER_HORROR:
            processing = &horror;
            break;
        case WORKER_FANTASY:
            processing = &fantasy;
            break;
        case WORKER_COMEDY:
            processing = &comedy;
            break;
        case WORKER_SCIENCE_FICTION:
            processing = &sf;
            break;
    }
    // out of bounds check
    if (end > paragraph.size()) {
        end = paragraph.size();
    }
    // facem nr de iteratii asignate
    for (int i = 0; i < iterations; i++) {
        processing(paragraph.begin() + start, paragraph.begin() + end);

        // next iteration bounds
        start += (max_threads - 1) * 20;
        end = start + 20;

        if (end > paragraph.size()) {
            end = paragraph.size();
        }
    }
}
// "main"-ul workerului
void process_data(int rank, vector<string> &paragraph) {
    vector<thread> threads;
    // vedem cate chunk-uri avem
    int number_of_tasks = paragraph.size() / 20;
    // daca avem un chunk incomplet
    if (paragraph.size() % 20 != 0) {
        number_of_tasks++;
    }
    
    // cate thread-uri am avea nevoie maxim
    int threads_number = (max_threads - 1);
    // cate iteratii face obligatoriu fiecare thread
    int iterations_for_all = number_of_tasks / (max_threads - 1);
    // cate thread-uri impart iteratiile in plus
    int threads_with_more_iterations = number_of_tasks % (max_threads - 1);

    // daca nu avem nevoie de toate thread-urile
    if (number_of_tasks < max_threads) {
        threads_number = number_of_tasks;
        iterations_for_all = 0;
        threads_with_more_iterations = number_of_tasks;
    }

    // pornim thread-urile
    for (int i = 0; i < threads_number; i++) {
        int start = i * 20;
        int end = start + 20;
        
        if (i < threads_with_more_iterations) {
            threads.push_back(thread(worker_thread_function, rank, ref(paragraph), start, end, (iterations_for_all + 1)));
        } else {
            threads.push_back(thread(worker_thread_function, rank, ref(paragraph), start, end, iterations_for_all));
        }
    }

    // join
    for (int i = 0; i < threads_number; i++) {
        threads[i].join();
    }
}

// functia care se ocupa de send/recv in worker
// care apeleaza process_data
void master_thread(int rank) {
    vector<string> paragraph(0);
    bool done = false;
    int number_of_lines;
    MPI_Status status;

    while (!done) {
        MPI_Recv(&number_of_lines, 1, MPI_INT, 0, rank, MPI_COMM_WORLD, &status);

        // work is done
        if (number_of_lines == -1) {
            done = true;
            break;
        }

        paragraph.clear();

        // receive paragraph
        for (int i = 0; i < number_of_lines; i++) {
            MPI_Probe(0, rank, MPI_COMM_WORLD, &status);

            int line_size;
            MPI_Get_count(&status, MPI_CHAR, &line_size);

            char *line = (char *)calloc(sizeof(char), line_size);
            MPI_Recv(line, line_size, MPI_CHAR, 0, rank, MPI_COMM_WORLD, &status);
            paragraph.push_back(string(line));
        }

        process_data(rank, paragraph);

        // send data back
        // trimitem linie cu linie
        for (int line = 0; line < paragraph.size(); line++) {
            MPI_Send(paragraph[line].c_str(), paragraph[line].length() + 1, MPI_CHAR, 0, rank, MPI_COMM_WORLD);
        }
    }
}

int main(int argc, char *argv[]) {
    init_consonant_table();
    max_threads = thread::hardware_concurrency();
    int numtasks, rank;
    vector<thread> threads;

    int provided;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    if (rank == 0) {
        string fileName = argv[1];

        for (int id = 0; id < 4; id++) {
            threads.push_back(thread(read_file, id, fileName));
        }

        for (int id = 0; id < 4; id++) {
            threads[id].join();
        }
    } else {
        thread w_thread(master_thread, rank);
        w_thread.join();
    }

    MPI_Finalize();

}

