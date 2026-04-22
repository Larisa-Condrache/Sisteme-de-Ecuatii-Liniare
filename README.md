# T26 Linear System Solver

Server multi-threaded în C cu trei canale de comunicație concurente (UNIX / INET / SOAP), integrat cu **libconfig** pentru configurare dinamică la runtime, **getopt** pentru argumente CLI, și un motor de calcul paralel hibrid pentru rezolvarea sistemelor de ecuații liniare (T26).

---

## Cuprins

1. [Capabilități principale (Ce face proiectul)](#capabilități-principale-ce-face-proiectul)
2. [Structura proiectului](#structura-proiectului)
3. [Dependențe](#dependențe)
4. [Compilare](#compilare)
5. [Configurare](#configurare)
6. [Rulare și Exemple de Utilizare](#rulare-și-exemple-de-utilizare)
7. [Protocolul binar INET](#protocolul-binar-inet)
8. [T26 — Solver sisteme liniare (Arhitectura Matematică)](#t26--solver-sisteme-liniare-arhitectura-matematică)

---

## 1. Capabilități principale (Ce face proiectul)

Aplicația este un ecosistem client-server robust, capabil să proceseze cereri matematice și operațiuni generale de rețea. Funcționalitățile sale cheie sunt:

*   **Parsare Algebrică Umană:** Clientul dedicat (`equation_type1_client`) permite utilizatorului să introducă ecuații naturale din terminal, de tipul `"2x+3y=7"`, `"x-y=-1"`. Clientul extrage automat matricea $A$ și vectorul $b$.
*   **Solver Liniar Paralel Propriu:** Pentru sisteme mari, serverul nu deleagă pur și simplu munca. Implementează un **algoritm custom de Eliminare Gaussiană Paralelă** cu comunicare IPC (pipe-uri bidirecționale). Procesul părinte instanțiază Workeri (`fork`), le împarte benzile de calcul și le sincronizează asincron.
*   **Fallback Serial Optimizat:** Pentru sisteme mici ($n < 32$), serverul ocolește intenționat overhead-ul de `fork()` și apelează direct funcția ultra-optimizată `LAPACKE_dgesv` (OpenBLAS).
*   **Server Multi-Protocol:**
    *   **INET:** Multiplexează prin `select()` zeci de clienți simultan pe TCP (port implicit 18081).
    *   **UNIX:** Ocolește stiva de rețea pentru performanță maximă pe aceeași mașină (`/tmp/unixds`).
    *   **SOAP:** Oferă o interfață Enterprise Web Service pentru clienți scriși în alte limbaje (ex. Java, C#).
*   **Configurare ierarhică:** Serverul își preia setările combinând argumente CLI (`--port`), Variabile de Mediu (`PCD_CONFIG`) și fișiere `.cfg` (`libconfig`).

---

## 2. Structura proiectului

```text
skeleton/
├── server.cfg               # Fișier de configurare libconfig
├── config.h / .c            # Implementare încărcare și merge configurări
├── threeds.c                # Entry-point server: parsează CLI, execută demo fork(), pornește firele
├── inetds2.c                # Thread server INET/TCP (select-based multiplexer)
├── unixds.c                 # Thread server UNIX Domain Sockets
├── soapds.c                 # Thread server SOAP (via gsoap)
├── proto.h / .c             # Definiții protocol binar INET + funcții serializare (read/write)
├── solver.h / .c            # Implementare T26: Eliminare Gaussiană Paralelă + serial LAPACKE
├── inetsample2.c            # Client generic INET (testează ECHO, CONC, ADD, NEG)
├── equation_type1_client.c  # Client avansat: parsează ecuații string și cere serverului OPR_SOLVE
├── sclient.h                # Definiții servicii gsoap (input pentru soapcpp2)
└── Makefile
```

---

## 3. Dependențe

Pentru a compila proiectul, sistemul are nevoie de librăriile gSOAP, libconfig și mediul matematic LAPACKE/OpenBLAS.

```bash
sudo apt-get update
sudo apt-get install -y \
    gsoap libgsoap-dev \
    libconfig-dev \
    liblapacke-dev \
    libopenblas-dev
```

---

## 4. Compilare

```bash
# Compilează tot: generare SOAP, Server, Client Ecuații, Client INET
make all

# Compilare specifică
make serverds
make equation_type1_client
make inetclient

# Curățare artefacte de build (și fișiere generate de gSOAP)
make clean
```

---

## 5. Configurare

Serverul citește parametrii din fișierul `server.cfg` prin biblioteca **libconfig**. Orice parametru lipsă este înlocuit intern de o valoare *fallback* compilată în C.

```cfg
server:
{
    inet_port   = 18081;
    soap_port   = 18082;
    unix_socket = "/tmp/unixds";
    backlog     = 10;
    reuse_addr  = 1;
};
logging:
{
    level       = 1;   # 0=erori, 1=info, 2=debug
};
solver:
{
    workers          = 4;   # Câți workeri (procese fork) rezolvă o matrice concomitent
    serial_threshold = 32;  # Dacă N < 32, ocolește fork() și folosește algoritmul serial LAPACKE
};
```

Puteți suprascrie configurațiile direct din **CLI**:
`./serverds --port 9090 --soap-port 8080 --log-level 2`

Sau folosind **Variabile de Mediu**:
`PCD_CONFIG=alt_fisier.cfg PCD_LOG_LEVEL=2 ./serverds`

---

## 6. Rulare și Exemple de Utilizare

### Pornirea Serverului

```bash
./serverds
```

### Clientul de Ecuații Liniar (Demonstrația Principală)

Clientul suportă sisteme de ordinul 1, 2 sau 3 (date ca argumente text). Sistemul parsează ecuațiile, le transformă într-o matrice binară și roagă serverul să o calculeze.

**Sistem de Ordin 2:**
```bash
./equation_type1_client -n 2 "2x+3y=7" "x+2y=4"
```
*Output așteptat:*
```text
[client] Sistem de ordin 2:
  Ec 1: 2x +3y = 7
  Ec 2: 1x +2y = 4
[client] Conectat la 127.0.0.1:18081

[REZULTAT]
  x = 2.000000
  y = 1.000000
```

**Sistem de Ordin 3 (cu coeficienți impreciși/negativi):**
```bash
./equation_type1_client -n 3 "x+y+z=6" "2x-y+z=3" "x+2y-z=2"
```

### Clientul Generic INET

Acest client se conectează la server și testează serializarea primară, concatenarea stringurilor și operațiile matematice elementare.
```bash
./inetclient
./inetclient --host 127.0.0.1 --port 18081
```

---

## 7. Protocolul binar INET (FCE)

Protocolul personalizat funcționează prin trimiterea unui **Header fix de 12 bytes** la începutul oricărui mesaj. Valorile din header sunt formatate conform **Network Byte Order**.

```text
┌──────────────┬──────────────┬──────────────┐
│  msgSize     │  clientID    │  opID        │
│  (int32_t)   │  (int32_t)   │  (int32_t)   │
└──────────────┴──────────────┴──────────────┘
```

Operațiunea centrală este `OPR_SOLVE = 6`. 
1. Clientul transmite: `[Header]` + `[n (ordinul)]` + `[Matricea A n*n doubles]` + `[Vector b n doubles]`.
2. Serverul răspunde: `[Header]` + `[Status]` + `[Vector x n doubles]`. (Valorile double utilizează arhitectura nativă IEEE 754).

---

## 8. T26 — Solver sisteme liniare (Arhitectura Matematică)

Când serverul primește cererea `OPR_SOLVE` pe thread-ul INET, acesta apelează `solver_solve()` din `solver.c`.

### A. Calea Paralelă (Eliminare Gaussiană Multi-Proces)
Dacă ordinul `N >= serial_threshold` și avem > 1 workers alocați:
1. Părintele construiește o matrice augmentată $[A|b]$.
2. Părintele folosește `pipe()` și creează $W$ Workeri (`fork()`).
3. Matricea este împărțită pe orizontală în $W$ benzi. Fiecărui proces copil îi este asigurat un set de rânduri pe care va lucra.
4. **Eliminarea:** Se iterează prin $n$ pași. Procesul care deține "rândul pivot" la pasul curent îl transmite Părintelui, care face un *broadcast* în pipe-uri către ceilalți workeri. Aceștia reduc elementele de sub diagonala principală pe benzile lor.
5. La final, Părintele recuperează matricea superior-triunghiulară și rezolvă substitutia înapoi (Back-Substitution) pentru a genera răspunsul.

### B. Calea Serială (Fallback Stabil)
Dacă ecuația este prea mică ($N < 32$), overhead-ul sistemului de operare (schimb de context IPC/fork) nu se justifică. Astfel, sistemul rezolvă matematic matrizile delegând procesarea către `LAPACKE_dgesv` (o rutină din OpenBLAS extrem de stabilă numeric pentru descompunere LU).
