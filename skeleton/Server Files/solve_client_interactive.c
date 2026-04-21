/*
  Verificare build si analiza statica:
    • -Wall si -Wextra semnaleaza probleme generale precum variabile
      nefolosite, formate gresite sau apeluri suspecte.
    • -Wpedantic forteaza respectarea mai stricta a standardului C.
    • -Wshadow avertizeaza daca o variabila locala ascunde alta variabila,
      situatie care poate produce bug-uri greu de observat.
    • -Wconversion avertizeaza la conversii implicite care pot pierde
      informatie sau pot schimba semnul unei valori.
    • -Wformat=2 verifica mai strict folosirea functiilor din familia printf.
    • -Wnull-dereference semnaleaza dereferentieri potential invalide.
    • Fisierul este inclus si in tinta „make tidy”, unde clang-tidy face
      o analiza statica suplimentara pentru stil, siguranta si bune practici.
 */

/*
 * solve_client_interactive.c — Interactive client for OPR_SOLVE (T26).
 *
 * Reads the linear system Ax=b from stdin, sends it to the PCD server,
 * and prints the solution.
 *
 * Usage:
 *   ./solve_client_interactive [-h host] [-p port]
 *
 * Build:
 *   gcc solve_client_interactive.c proto.c -o solve_client_interactive -Wall -Wextra
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "proto.h"

#define DEFAULT_HOST    "127.0.0.1"
#define DEFAULT_PORT    18081

static void read_system_from_stdin(int *out_n, double **out_A, double **out_b)
{
    int n;
    printf("Introduceti ordinul sistemului (n): ");
    if (scanf("%d", &n) != 1 || n <= 0 || n > 4096) {
        fprintf(stderr, "n invalid (trebuie sa fie intre 1 si 4096)\n");
        exit(EXIT_FAILURE);
    }

    double *A = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *b = (double *)malloc((size_t)n       * sizeof(double));
    if (!A || !b) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Introduceti matricea A (%dx%d), rand cu rand:\n", n, n);
    for (int i = 0; i < n; i++) {
        printf("  Randul %d: ", i + 1);
        for (int j = 0; j < n; j++) {
            if (scanf("%lf", &A[i * n + j]) != 1) {
                fprintf(stderr, "Eroare la citirea elementului A[%d][%d]\n", i, j);
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("Introduceti vectorul b (%d valori):\n  ", n);
    for (int i = 0; i < n; i++) {
        if (scanf("%lf", &b[i]) != 1) {
            fprintf(stderr, "Eroare la citirea elementului b[%d]\n", i);
            exit(EXIT_FAILURE);
        }
    }

    *out_n = n;
    *out_A = A;
    *out_b = b;
}

static void init_sockaddr(struct sockaddr_in *name,
                          const char *hostname, uint16_t port)
{
    struct hostent *hostinfo = gethostbyname(hostname);
    if (!hostinfo) {
        fprintf(stderr, "Host necunoscut: %s\n", hostname);
        exit(EXIT_FAILURE);
    }
    name->sin_family = AF_INET;
    name->sin_port   = htons(port);
    name->sin_addr   = *(struct in_addr *)hostinfo->h_addr_list[0];
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-h host] [-p port]\n"
            "  -h  Adresa server  [implicit: %s]\n"
            "  -p  Port server    [implicit: %d]\n",
            prog, DEFAULT_HOST, DEFAULT_PORT);
}

int main(int argc, char *argv[])
{
    const char *host = DEFAULT_HOST;
    int         port = DEFAULT_PORT;

    static const struct option long_opts[] = {
        { "host", required_argument, NULL, 'h' },
        { "port", required_argument, NULL, 'p' },
        { NULL, 0, NULL, 0 }
    };
    int c;
    while ((c = getopt_long(argc, argv, "h:p:", long_opts, NULL)) != -1) {
        switch (c) {
        case 'h': host = optarg;       break;
        case 'p': port = atoi(optarg); break;
        default:  print_usage(argv[0]); return EXIT_FAILURE;
        }
    }

    /* ---- Citire sistem de la tastatura -------------------------------- */
    int     n = 0;
    double *A = NULL;
    double *b = NULL;
    read_system_from_stdin(&n, &A, &b);

    fprintf(stderr, "[client] Sistem de ordin n=%d trimis catre server.\n", n);

    /* ---- Conectare ---------------------------------------------------- */
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in srv;
    init_sockaddr(&srv, host, (uint16_t)port);
    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(sock); return EXIT_FAILURE;
    }
    fprintf(stderr, "[client] Conectat la %s:%d\n", host, port);

    /* ---- Handshake ---------------------------------------------------- */
    msgHeaderType h;
    msgIntType    m;
    h.clientID = 0; h.opID = OPR_CONNECT;
    writeSingleInt(sock, h, 0);
    readSingleInt(sock, &m);
    int clientID = m.msg;
    h.clientID   = clientID;
    fprintf(stderr, "[client] ClientID = %d\n", clientID);

    /* ---- OPR_SOLVE ---------------------------------------------------- */
    h.opID = OPR_SOLVE;
    fprintf(stderr, "[client] Trimit cererea OPR_SOLVE...\n");

    if (writeSolveRequest(sock, h, n, A, b) < 0) {
        fprintf(stderr, "[client] writeSolveRequest esuat\n");
        close(sock); return EXIT_FAILURE;
    }

    int     status = 0;
    double *x      = NULL;
    if (readSolveResponse(sock, &status, &n, &x) < 0) {
        fprintf(stderr, "[client] readSolveResponse esuat\n");
        close(sock); return EXIT_FAILURE;
    }

    if (status != 0) {
        fprintf(stderr, "[client] Serverul a returnat eroare (status=%d)."
                " Sistemul poate fi singular.\n", status);
        close(sock); return EXIT_FAILURE;
    }

    /* ---- Afisare solutie ---------------------------------------------- */
    fprintf(stderr, "[client] Solutie primita:\n");
    printf("x = [\n");
    for (int i = 0; i < n; i++)
        printf("  x[%d] = %12.6f\n", i, x[i]);
    printf("]\n");

    free(x); free(A); free(b);

    /* ---- BYE ---------------------------------------------------------- */
    h.opID = OPR_BYE;
    writeSingleInt(sock, h, 0);
    close(sock);

    return EXIT_SUCCESS;
}
