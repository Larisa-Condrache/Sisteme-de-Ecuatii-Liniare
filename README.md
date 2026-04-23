# PCD 22-23 IA3 — Milestone 1 + T26 Linear System Solver

Server multi-threaded în C care suportă trei tipuri de comunicație (UNIX / INET / SOAP), utilizează **libconfig** pentru configurare și implementează un solver pentru sisteme de ecuații liniare.

---

## Cuprins

1. Structura proiectului
2. Dependențe
3. Compilare
4. Configurare
5. Rulare
6. Arhitectura serverului
7. Protocolul binar INET
8. T26 — Solver sisteme liniare
9. Demo Milestone 1

---

## Structura proiectului

```text
├── server.cfg              # Fișier de configurare (libconfig)
├── config.h
├── config.c               # Citire și aplicare configurare
├── threeds.c              # main(): inițializare server
├── inetds2.c              # Server TCP (procesează cereri)
├── unixds.c               # Server UNIX domain socket
├── soapds.c               # Server SOAP
├── proto.h
├── proto.c                # Definire protocol și funcții de comunicare
├── solver.h
├── solver.c               # Implementare solver sisteme liniare
├── equation_type1_client.c# Client de test pentru operații (inclusiv solver)
├── sclient.h              # Definiții pentru SOAP
├── openapi_pcd.yaml       # Specificație API
└── makefile
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

---

## Compilare

```bash
make all
make clean
```

---

## Configurare

Configurarea serverului se realizează prin fișierul `server.cfg`, care definește:

* porturile pentru comunicația TCP și SOAP
* calea pentru socket-ul UNIX
* parametri de logare
* parametri pentru solver (număr de procese, prag serial)

Exemplu:

```cfg
server:
{
    inet_port   = 18081;
    soap_port   = 18082;
    unix_socket = "/tmp/unixds";
};

solver:
{
    workers          = 4;
    serial_threshold = 32;
};
```

Biblioteca **libconfig** este utilizată exclusiv pentru citirea acestor parametri și configurarea aplicației.

---

## Rulare

### Server

```bash
./serverds
```

### Client de test

```bash
./equation_type1_client
```

---

## Arhitectura serverului

```text
main() — threeds.c
│
├── config_load()
├── config_apply_args()
│
├── pthread_create → unix_main()
├── pthread_create → inet_main()
└── pthread_create → soap_main()
```

---

## Protocolul binar INET

Comunicarea TCP se realizează printr-un protocol binar care conține:

* dimensiunea mesajului
* identificatorul clientului
* codul operației

### Operații suportate

| Cod | Operație |
| --- | -------- |
| 0   | CONNECT  |
| 1   | ECHO     |
| 2   | CONCAT   |
| 3   | NEG      |
| 4   | ADD      |
| 5   | BYE      |
| 6   | SOLVE    |

---

## T26 — Solver sisteme liniare

Se rezolvă sisteme de forma:

```
Ax = b
```

---

### Implementare

* logica este implementată în `solver.c`
* este apelată din `inetds2.c` la primirea unei cereri `SOLVE`
* comunicarea datelor se realizează prin funcțiile din `proto.c`

---

### Metodă utilizată

* `LAPACKE_dgesv` (descompunere LU)
* complexitate: O(n³)

---

### Moduri de execuție

#### Execuție serială

* utilizată pentru sisteme mici

#### Execuție paralelă

* utilizată pentru sisteme mari
* mecanisme:

  * `fork()`
  * `pipe()`

Procesele copil calculează parțial soluția și transmit rezultatele către procesul părinte.

---

## Fluxul de execuție

```text
equation_type1_client.c
   ↓
inetds2.c
   ↓
solver.c
   ↓
inetds2.c
   ↓
equation_type1_client.c
```

---

## Demo Milestone 1

La pornire, serverul execută un exemplu demonstrativ folosind:

* `fork()`
* `pipe()`
* `wait()`

Valorile utilizate sunt citite din `server.cfg`.

---

## Concluzie

Proiectul integrează:

* programare concurentă (thread-uri și procese)
* comunicare inter-proces (IPC)
* protocoale de rețea
* configurare dinamică
* calcul numeric

Structura modulară separă clar componentele de configurare, comunicație și calcul.
