# Distributed Text Processing
Homework for Parallel and Distributed Algorithms. An application that processes paragraphs according to some rules (irrelevant) using MPI.

# Romanian readme

## Introducere

Pentru tema asta am incercat sa modularizez codul pe cat posibil. Pentru
asta, s-ar putea ca performanta sa sufere, din cauza apelurilor de functii
multiple.

Am ales sa implementez rezolvarea in C++, deoarece ma simt mai in largul
meu lucrand cu `std::string`, `std::vector`, `std::thread` si 
`std::condition_variable`.

## Citirea

Pentru citire, am scris o functie `send_and_recv`, care este executata
in paralel de fiecare thread de pe MASTER.

Fiecare thread se ocupa de un anume gen de paragraf:

| Master Thread | Worker | Genre |
|---------|----------|------------------- |
|Thread 0 | Worker 1 | horror |
|Thread 1 | Worker 2 | comedy |
|Thread 2 | Worker 3 | fantasy |
|Thread 3 | Worker 4 | science-fiction |



Functia executata de thread-uri urmeaza logica:

```
Cat timp putem citi
    
    Thread-ul 0 memoreaza ordinea genurilor

    Daca am dat de un \n, inseamna ca avem paragraf nou,
    deci am terminat paragraful curent
    
        Daca la pasii anteriori am memorat paragraful bun
        (asta face "found", e true doar daca am dat de paragraful
        thread-ului respectiv)
    
            Trimitem la worker

            Primim de la worker
            
            Asteptam sa fie randul nostru la depozitatul datelor
    
            Dupa ce ne-am facut randul, anuntam pe toti ca am terminat

    Daca citim paragraful nostru si nu e newline
        Adaugam la paragraf

    Daca am gasit numele genului
        Setam ca la urmatoarea iteratie sa incepe sa memoram
```

Ca sa nu avem probleme de sincronizare, am pus o bariera, ca sa asteptam
toate thread-urile sa primeasca datele procesate. Pentru ca in C++
nu avem bariera (decat in C++20, dar nu am reusit sa conving mpic++
sa compileze cu alt gcc), am folosit un `condition variable`, care
lasa thread-urile sa iasa din `wait()` doar daca `finishedThreads == 4`,
adica daca toate 4 thread-urile au dat `wait()` pe variabila.

Din cauza modului in care e facuta functia, nu putem detecta in `while`
daca avem un paragraf la final. Astfel, adaugam cazul in care:
Daca paragraful nu este gol, trimitem si primim de la worker.

Dupa ce am terminat de comunicat cu workerii, le trimitem `-1` ca sa 
ii anuntam ca am terminat.



## Prelucrarea textului

Mi-am facut o functie pentru fiecare tip de prelucrare. Mi s-a parut
cel mai usor sa dau functiei doar 2 iteratori (start si end) ca sa
abstractizez practic paragraful. Fiecare functie de prelucrare vede
doar particica ei.

## Worker threads

Pentru worker, am cateva functii ajutatoare:

### master_thread

Functia asta se ocupa doar de primit si trimis date. De mentionat ca am
alocat totul dinamic, cu `calloc`, folosind `MPI_Probe` ca sa extrag
dimensiunea liniei

### process_data

Functia asta se ocupa de impartirea paragrafului pe thread-uri. Sper ca
variabilele sa fie denumite destul de intuitiv.
De mentionat ca am pornit o singura data thread-urile necesare per
paragraf. Fiecare thread primeste `iterations`, care practic ii spune
cate chunk-uri de 20 are de prelucrat.
In cazul cu 8 thread-uri (ce am eu local, mersi hyperthreading), fiecare
chunk este la distanta de 140 fata de primul. Adica am incercat sa impart
echitabil chunk-urile.

De exemplu, daca am 151 de linii:

| Thread id | Number of chunks | Start | End |
|-----------|------------------|-------|-----|
| 0         | 2                | 0     | 20  |
| 1         | 2                | 21    | 40  |
| 2         | 1                | 41    | 60  |
| 3         | 1                | 61    | 80  |
| 4         | 1                | 81    | 100 |
| 5         | 1                | 101   | 120 |
| 6         | 1                | 121   | 140 |
| 7         | 1                | 141   | 151 |


In functia mea, iterations_for_all ar fi 1 (minumul de iteratii
pe care il face fiecare thread), iar threads_with_more_iterations
ar fi 2, pentru ca thread-ul 0 si 1 au cu 1 mai multe iteratii decat
celelalte. Number of tasks este doar numarul de chunks, in cazul asta
ar fi 10.

Pentru a nu primi `Segmentation fault`, am adaugat si verificarea ca
`end` sa nu depaseasca dimensiunea paragrafului.

### worker_thread_function

Functia asta este functia apelata de fiecare thread din worker. Scopul
ei este sa isi determine functia de prelucrare apelata (prin function
pointer-ul `processing`) si sa isi faca numarul de iteratii.

### is_consonant & init_consonant_table

Din considerente de performanta, am vrut sa fac verificarea
daca o litera este consoana in O(1), deoarece se va face de o groaza
de ori per rulare. Astfel, mi-am facut un array de la 'a' la 'Z', care
are valoarea `true` pentru consoane si `false` pentru vocale.

