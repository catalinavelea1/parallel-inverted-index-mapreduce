
-- initializari --
    Sunt procesate datele primite ca argumente si introduse in structura
GlobalData, care contine toate variabilele care ar fi fost folosite la nivel
global. Se foloseste vectorul de perechi thread_args pentru a separa
argumentele specifice fiecarui thread. Un camp pentru id-ul firului de executie
nu a fost inclus in structura GlobalData pentru a se evita suprascrierea, fiind
o variabila la care nu vrem sa aiba acces toate threadurile concomitent.

-- get_files --
    Se proceseaza fisierul primit la intrare, care contine toate celelalte
fisiere, din care vom extrage cuvintele. Aceste fisiere sunt introduse intr-un
queue, pentru a putea fi distribuite dinamic intre mappers.

-- map_files --
    Se va completa vectorul de liste partiale, care a primit dimensiunea exacta
M, deoarece fiecare mapper genereaza lista sa. Se creeaza cate un array
de cuvinte local, pentru a evita un overhead produs de lock si unlock, in cazul
in care am fi lucrat direct cu partial_lists, la care au acces toate
threadurile. Este totusi nevoie de lock atunci cand fiecare thread preia un
fisier din coada. Astfel, in timp ce un thread este ocupat procesand un fisier
mai mare, celelalte pot continua extragerea din coada, procesand fisiere mai
mici in paralel. La acest pas se creeaza doar perechile cuvant fisier, sortatul
si eliminarea duplicatelor urmand sa fie rezolvate in pasii viitori.
    Tot la acest pas caracterele fiecarui cuvant sunt transformate astfel incat
exista doar litere mici si sunt eliminate alte caractere precum _ sau '.

-- reduce_files --
    Din vectorul de liste partiale se distribuie anumite bucati intre reducers.
Fiecare thread va copia perechile din listele partiale intr-un map local. Este
folosit un map pentru a se elimina duplicatele. Am ales tot o variabila
locala, pentru ca nr de lock si unlock ar fi fost foarte mare daca am fi lucrat
direct cu mapul global, fiind mai multe iteratii la acest pas, in comparatie cu
pasul in care se copiaza cuvinte din mapul local in cel global. Acesta este si
pasul in care se creeaza un set de indexi ai fisierelor.
    In acest punct, fiecare fir de executie de tip reduce a generat propriul
map specific segmentului sau de cuvinte. Se foloseste lock deoarece fiecare
fir de executie doreste sa copieze perechile cuvant - id in mapul global care
reprezinta o zona critica. Am folosit tot un map deoarece atunci cand inseram
un cuvant acesta va fi gasit in map, daca exista deja, si se va completa
setul sau de indexi automat. Pentru indexi am folosit set cu scopul de a fi
sortati crescator atunci cand sunt adaugati si a se elimina duplicatele.
    Se obtine words_map care contine toate cuvintele cu setul complet de
indexi, dar trebuie sa ne asiguram ca toate threadurile si-au procesat
portiunea pana trecem la prelucrarea mapului, asa ca folosim bariera.
    Pentru a sorta cuvintele care incep cu aceeasi litera in functie de nr de
indexi, le mutam in vectori specifici fiecarei litere ale alfabetului, pe care
ii sortam corespunzator.
    Acesti vectori sunt apoi impartiti din nou intre threaduri si sunt copiate
cuvintele in fisierele specifice literei.

-- lucru cu bariere --
    Am folosit o bariera separata de dimensiune M + R, pentru a ma asigura ca
firele de executie de tip reducer nu isi vor incepe executia pana cand toate
firele de tip map nu isi termina la randul lor sarcinile. Bariera de dimensiune
R este folosita doar in cadrul operatiilor de tip reduce.
