# PCD 22-23 IA3 — Milestone 1 + T26 Linear System Solver

Server multi-threaded în C cu trei canale de comunicație (UNIX / INET / SOAP), integrat cu **libconfig** pentru configurare la runtime, **getopt** pentru argumente CLI, și **LAPACKE** (OpenBLAS) pentru rezolvarea paralelă a sistemelor de ecuații liniare (T26).

---

## Cuprins

1. [Structura proiectului](#structura-proiectului)
2. [Dependențe](#dependențe)
3. [Compilare](#compilare)
4. [Configurare](#configurare)
5. [Rulare](#rulare)
6. [Arhitectura serverului](#arhitectura-serverului)
7. [Protocolul binar INET](#protocolul-binar-inet)
8. [T26 — Solver sisteme liniare](#t26--solver-sisteme-liniare)
9. [Demo Milestone 1](#demo-milestone-1)
10. [Modificări față de skeleton](#modificări-față-de-skeleton)

---

## Structura proiectului

```
skeleton/
├── server.cfg        # Fișier de configurare libconfig
├── config.h          # Structura server_config_t și API-ul de configurare
├── config.c          # Implementare libconfig: config_load(), config_apply_args()
├── threeds.c         # main(): CLI (getopt), env vars, fork() demo, pornire threaduri
├── inetds2.c         # Thread server INET/TCP (select-based multiplexer)
├── unixds.c          # Thread server UNIX domain socket
├── soapds.c          # Thread server SOAP (via gsoap)
├── proto.h           # Definiții protocol binar + structuri OPR_SOLVE
├── proto.c           # Helpere read/write pentru toate tipurile de mesaje
├── solver.h          # API solver: solver_solve(n, A, b, num_workers)
├── solver.c          # Implementare: serial (LAPACKE_dgesv) + parallel (fork+pipe)
├── inetsample2.c     # Client de test general INET cu getopt
├── solve_client.c    # Client de test dedicat T26 (OPR_SOLVE)
├── sclient.h         # Definiții servicii gsoap (input pentru soapcpp2)
└── makefile

soap-sample/          # Exemplu complet furnizat de profesor (nemodificat)
└── SOAP_CLIENT/      # Client HTML/JS pentru testare SOAP din browser
```

---

## Dependențe

```bash
sudo apt-get update
sudo apt-get install -y \
    gsoap libgsoap-dev \
    libconfig-dev \
    liblapacke-dev \
    libopenblas-dev
```

Sau direct prin target-ul din makefile:

```bash
make deps
```

---

## Compilare

```bash
# Server + ambii clienți de test
make all

# Doar serverul
make serverds

# Doar clientul INET general
make inetclient

# Doar clientul T26
make solve_client

# Curățare artefacte de build
make clean
```

> **Notă:** Prima compilare generează automat fișierele gsoap (`soapC.c`, `soapServer.c`, etc.) prin `soapcpp2 -S -c -x sclient.h`.

---

## Configurare

Serverul citește `server.cfg` la pornire via **libconfig**. Orice cheie lipsă din fișier este înlocuită cu valoarea implicită compilată în `config.c`.

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
    destination = "stderr";
};

demo:
{
    input_string = "Hello from libconfig!";
    input_int1   = 42;
    input_int2   = 58;
};

solver:
{
    workers          = 4;   # procese copil pentru rezolvare paralelă
    serial_threshold = 32;  # sub acest n se rezolvă serial (fără fork)
};
```

### Argumente CLI

Argumentele CLI suprascriu valorile din fișierul de configurare.

| Flag scurt | Flag lung | Descriere | Default |
|---|---|---|---|
| `-c` | `--config <path>` | Calea către fișierul .cfg | `server.cfg` |
| `-p` | `--port <n>` | Port INET/TCP | `18081` |
| `-s` | `--soap-port <n>` | Port SOAP | `18082` |
| `-u` | `--unix-socket <path>` | Cale socket UNIX | `/tmp/unixds` |
| `-l` | `--log-level <0\|1\|2>` | Verbozitate log | `1` |
| `-h` | `--help` | Afișează help | — |

### Variabile de mediu

| Variabilă | Efect |
|---|---|
| `PCD_CONFIG` | Suprascrie calea default a fișierului de configurare |
| `PCD_LOG_LEVEL` | Setează log level-ul înainte de parsarea CLI |

**Ordinea de prioritate:** CLI > variabile de mediu > `server.cfg` > valori implicite

---

## Rulare

### Server

```bash
# Rulare cu configurare implicită
./serverds

# Port INET custom + log debug
./serverds --port 9090 --log-level 2

# Fișier de configurare alternativ
./serverds --config /etc/pcd/myserver.cfg

# Via variabile de mediu
PCD_CONFIG=myserver.cfg PCD_LOG_LEVEL=2 ./serverds
```

### Client INET general

```bash
./inetclient
./inetclient --host 192.168.1.10 --port 18081
```

### Client T26 — Linear System Solver

```bash
# Sistem 4×4 (default) — soluție afișată cu eroare numerică
./solve_client

# Sistem 200×200
./solve_client -n 200

# Serverul pe altă mașină
./solve_client -h 192.168.1.10 -p 18081 -n 100
```

---

## Arhitectura serverului

```
main() — threeds.c
│
├── config_load("server.cfg")      ← libconfig
├── config_apply_args(...)         ← CLI / env vars
├── demo_fork()                    ← fork() + pipe + wait()
│
├── pthread_create → unix_main()   ← UNIX domain socket  (:unix_socket)
├── pthread_create → inet_main()   ← TCP/INET select()   (:inet_port)
└── pthread_create → soap_main()   ← gsoap               (:soap_port)
```

---

## Protocolul binar INET

Fiecare mesaj are un **header fix de 12 bytes** urmat de un payload variabil:

```
┌──────────────┬──────────────┬──────────────┐
│  msgSize     │  clientID    │  opID        │
│  (4 bytes)   │  (4 bytes)   │  (4 bytes)   │
└──────────────┴──────────────┴──────────────┘
```

Valorile din header sunt în **network byte order**. Matricele/vectorii din OPR_SOLVE sunt în **native byte order** (IEEE 754 little-endian pe x86/ARM64 Linux).

### Operații

| Cod | Constantă | Request | Response |
|-----|-----------|---------|----------|
| 0 | `OPR_CONNECT` | `singleInt(0)` | `singleInt(clientID)` |
| 1 | `OPR_ECHO` | `singleString(s)` | `singleString(s)` |
| 2 | `OPR_CONC` | `singleString(s1)` + `singleString(s2)` | `singleString(s1+" "+s2)` |
| 3 | `OPR_NEG` | `singleInt(n)` | `singleInt(-n)` |
| 4 | `OPR_ADD` | `multiInt(a, b)` | `singleInt(a+b)` |
| 5 | `OPR_BYE` | `singleInt(0)` | *(conexiunea se închide)* |
| 6 | `OPR_SOLVE` | `solveRequestHeader` + `A[n×n]` + `b[n]` | `solveResponseHeader` + `x[n]` |

---

## T26 — Solver sisteme liniare

### Problema

Se rezolvă sistemul `Ax = b` unde `A` este o matrice densă `n×n` și `b` un vector de lungime `n`.

### Librăria externă: LAPACKE (OpenBLAS)

**LAPACKE** este interfața C pentru LAPACK. Funcția centrală:

```c
lapack_int LAPACKE_dgesv(
    int matrix_layout,   // LAPACK_ROW_MAJOR
    lapack_int n,        // ordinul sistemului
    lapack_int nrhs,     // 1 pentru un singur b
    double *A,           // matricea — suprascrisă cu factorii LU
    lapack_int lda,      // leading dimension = n
    lapack_int *ipiv,    // vector pivot (output)
    double *B,           // RHS — suprascris cu soluția x
    lapack_int ldb       // 1
);
```

Algoritmul intern este **descompunerea LU cu pivotare parțială**, cu complexitate `O(n³/3)`, numeric stabilă.

### Strategia de paralelizare (fork + pipe)

```
parent
  │
  ├── fork() → worker 0 → LAPACKE_dgesv pe banda [0,   n/4)  → pipe → x[0..n/4)
  ├── fork() → worker 1 → LAPACKE_dgesv pe banda [n/4, n/2)  → pipe → x[n/4..n/2)
  ├── fork() → worker 2 → LAPACKE_dgesv pe banda [n/2, 3n/4) → pipe → x[n/2..3n/4)
  └── fork() → worker 3 → LAPACKE_dgesv pe banda [3n/4, n)   → pipe → x[3n/4..n)
        │
        └── waitpid() pe toți → asamblare x complet
```

Fiecare worker primește întreaga matrice `A` și banda sa din `b`. Construiește un RHS de lungime `n` cu zeros în afara benzii proprii, apelează `LAPACKE_dgesv`, și trimite înapoi prin pipe doar intrările din banda sa din soluție.

**Calea serială** se folosește automat când `n < serial_threshold` (default 32) sau `workers = 1`.

### Format wire OPR_SOLVE

```
CLIENT → SERVER:
  [12 bytes] msgHeaderType    opID = 6
  [ 4 bytes] int32_t n        ordinul sistemului (network byte order)
  [8·n² bytes] double A[]    row-major, native byte order
  [8·n  bytes] double b[]    native byte order

SERVER → CLIENT:
  [12 bytes] msgHeaderType    opID = 6
  [ 4 bytes] int32_t status   0=ok | -1=singular | -2=alloc | -3=IPC
  [8·n  bytes] double x[]    doar dacă status == 0
```

### Exemplu output solve_client

```
[client] Connected to 127.0.0.1:18081
[client] ClientID = 1713612345
[client] System order n=4
[client] Expected solution x:
  1.0000  2.0000  3.0000  4.0000
[client] Sending OPR_SOLVE request...
x = [
  x[0] =     1.000000   (expected     1.000000, err = 3.55e-15)
  x[1] =     2.000000   (expected     2.000000, err = 7.11e-15)
  x[2] =     3.000000   (expected     3.000000, err = 0.00e+00)
  x[3] =     4.000000   (expected     4.000000, err = 4.44e-15)
]
Max absolute error: 7.105427e-15  ✓ PASS
```

---

## Demo Milestone 1

La fiecare pornire, înainte de a lansa thread-urile, serverul execută un demo `fork()`/`wait()` care ilustrează utilizarea libconfig, comunicarea prin pipe și recolectarea procesului copil:

```
[main] Running fork()/wait() demo...
[demo] fork()/wait() results from child (PID 12345):
       42 + 58 = 100
       -(42)   = -42
       echo    = "Hello from libconfig!"
```

Valorile provin din secțiunea `demo` a fișierului `server.cfg`.

---

## Modificări față de skeleton

| Fișier | Modificare |
|--------|-----------|
| `solver.h` / `solver.c` | **NOU** — solver T26: LAPACKE serial + fork paralel |
| `solve_client.c` | **NOU** — client de test dedicat OPR_SOLVE cu verificare numerică |
| `config.h` / `config.c` | **NOU** — integrare libconfig + câmpuri `solver_workers`, `solver_threshold` |
| `server.cfg` | **NOU** — fișier de configurare cu secțiunile server/logging/demo/solver |
| `proto.h` | Adăugat `OPR_SOLVE = 6`, structuri matrice, declarații helpere wire |
| `proto.c` | Adăugat 4 helpere pentru protocolul OPR_SOLVE |
| `inetds2.c` | Fix OPR_CONC (stub → implementat), fix malloc overflow, eliminat ncurses, adăugat `case OPR_SOLVE` |
| `threeds.c` | Rescris: getopt_long, getenv, config_load, demo_fork, pthread_create |
| `unixds.c` | SOCK_DGRAM → SOCK_STREAM, adăugat loop accept()/select() |
| `soapds.c` | Eliminat ncurses, unused-param warnings rezolvate |
| `inetsample2.c` | Adăugat getopt, test OPR_CONC, error handling complet |
| `makefile` | Adăugat `-llapacke -lopenblas -lm`, `solver.o`, target `solve_client` și `deps` |
