/* === COMENTARII EXPLICITE PENTRU PREZENTARE ===
 * Comentariile adaugate mai jos marcheaza explicit UNDE se intampla fiecare pas important:
 * initializare, conectare, handshake, citire cerere, procesare, rezolvare, raspuns si inchidere.
 * Logica programului NU este modificata; au fost adaugate doar explicatii in cod.
 */

/*
 * Verificare build si analiza statica:
 *   • Acest fisier se compileaza din Makefile cu warning-uri stricte,
 *     pentru a detecta din timp erori frecvente de programare si probleme
 *     de portabilitate.
 *   • -Wall si -Wextra semnaleaza probleme generale precum variabile
 *     nefolosite, formate gresite sau apeluri suspecte.
 *   • -Wpedantic forteaza respectarea mai stricta a standardului C.
 *   • -Wshadow avertizeaza daca o variabila locala ascunde alta variabila,
 *     situatie care poate produce bug-uri greu de observat.
 *   • -Wconversion avertizeaza la conversii implicite care pot pierde
 *     informatie sau pot schimba semnul unei valori.
 *   • -Wformat=2 verifica mai strict folosirea functiilor din familia printf.
 *   • -Wnull-dereference semnaleaza dereferentieri potential invalide.
 *   • Fisierul este inclus si in tinta „make tidy”, unde clang-tidy face
 *     o analiza statica suplimentara pentru stil, siguranta si bune practici.
 */

/* Trimite un sistem liniar Ax=b catre server si afiseaza solutia, apoi sistemul folosit implicit are o solutie analitica, astfel incat rezultatele pot fi verificate vizual.
 Utilizare:   ./solve_client [-h host] [-p port] [-n size] 
 Compilare:   gcc solve_client.c proto.c -o solve_client -Wall -Wextra 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "proto.h"

#define DEFAULT_HOST    "127.0.0.1"
#define DEFAULT_PORT    18081
#define DEFAULT_N       4


static void build_test_system(int n, double *A, double *b, double *x_expected)
{
    /* Completeaza A:  */
    /* AICI se construieste matricea de test A si se pregateste solutia asteptata. */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j)
                A[i * n + j] = (double)(2 * n);   /* diagonala */
            else
                A[i * n + j] = (double)(1 + (i + j) % 3);
        }
        x_expected[i] = (double)(i + 1);
    }

    /* AICI se calculeaza vectorul b astfel incat sistemul sa aiba solutia cunoscuta x_expected. */
    for (int i = 0; i < n; i++) {
        b[i] = 0.0;
        for (int j = 0; j < n; j++)
            b[i] += A[i * n + j] * x_expected[j];
    }
}


static void init_sockaddr(struct sockaddr_in *name,
                          const char *hostname, uint16_t port)
{
    struct hostent *hostinfo = gethostbyname(hostname);
    if (!hostinfo) {
        fprintf(stderr, "Unknown host: %s\n", hostname);
        exit(EXIT_FAILURE);
    }
    name->sin_family = AF_INET;
    name->sin_port   = htons(port);
    name->sin_addr   = *(struct in_addr *)hostinfo->h_addr_list[0];
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-h host] [-p port] [-n size]\n"
            "  -h  Server address  [default: %s]\n"
            "  -p  Server port     [default: %d]\n"
            "  -n  System order    [default: %d]\n",
            prog, DEFAULT_HOST, DEFAULT_PORT, DEFAULT_N);
}


int main(int argc, char *argv[])
{
    const char *host = DEFAULT_HOST;
    int         port = DEFAULT_PORT;
    int         n    = DEFAULT_N;

    static const struct option long_opts[] = {
        { "host", required_argument, NULL, 'h' },
        { "port", required_argument, NULL, 'p' },
        { "n",    required_argument, NULL, 'n' },
        { NULL, 0, NULL, 0 }
    };
    int c;
    /* AICI se parseaza argumentele clientului: host, port si dimensiunea sistemului. */
    while ((c = getopt_long(argc, argv, "h:p:n:", long_opts, NULL)) != -1) {
        switch (c) {
        case 'h': host = optarg;       break;
        case 'p': port = atoi(optarg); break;
        case 'n': n    = atoi(optarg); break;
        default:  print_usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (n <= 0 || n > 4096) {
        fprintf(stderr, "n must be in [1, 4096]\n");
        return EXIT_FAILURE;
    }

    /*  Alocare matrici */
    /* AICI clientul aloca memorie pentru A, b si solutia asteptata. */
    double *A          = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *b          = (double *)malloc((size_t)n       * sizeof(double));
    double *x_expected = (double *)malloc((size_t)n       * sizeof(double));
    if (!A || !b || !x_expected) {
        fprintf(stderr, "malloc failed\n");
        return EXIT_FAILURE;
    }

    build_test_system(n, A, b, x_expected);

    fprintf(stderr, "[client] System order n=%d\n", n);
    if (n <= 8) {
        fprintf(stderr, "[client] Expected solution x:\n  ");
        for (int i = 0; i < n; i++)
            fprintf(stderr, "%.4f  ", x_expected[i]);
        fprintf(stderr, "\n");
    }

    /*  Conectare  */
    /* AICI clientul creeaza socketul TCP pentru conexiunea catre server. */
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in srv;
    init_sockaddr(&srv, host, (uint16_t)port);
    /* AICI clientul se conecteaza la server. */
    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(sock); return EXIT_FAILURE;
    }
    fprintf(stderr, "[client] Connected to %s:%d\n", host, port);

    msgHeaderType h;
    msgIntType    m;
    /* AICI clientul incepe handshake-ul si cere serverului un clientID. */
    h.clientID = 0; h.opID = OPR_CONNECT;
    writeSingleInt(sock, h, 0);
    readSingleInt(sock, &m);
    int clientID  = m.msg;
    h.clientID    = clientID;
    fprintf(stderr, "[client] ClientID = %d\n", clientID);

    /* AICI clientul comuta pe operatia OPR_SOLVE. */
    h.opID = OPR_SOLVE;
    fprintf(stderr, "[client] Sending OPR_SOLVE request...\n");

    /* AICI clientul trimite sistemul liniar catre server. */
    if (writeSolveRequest(sock, h, n, A, b) < 0) {
        fprintf(stderr, "[client] writeSolveRequest failed\n");
        close(sock); return EXIT_FAILURE;
    }

    int     status = 0;
    double *x      = NULL;
    /* AICI clientul asteapta raspunsul serverului: status + vectorul solutie. */
    if (readSolveResponse(sock, &status, &n, &x) < 0) {
        fprintf(stderr, "[client] readSolveResponse failed\n");
        close(sock); return EXIT_FAILURE;
    }

    if (status != 0) {
        fprintf(stderr, "[client] Server returned error status %d\n", status);
        close(sock); return EXIT_FAILURE;
    }

    fprintf(stderr, "[client] Solution received.\n");

    /* AICI clientul compara numeric solutia primita cu solutia asteptata. */
    double max_err = 0.0;
    for (int i = 0; i < n; i++) {
        double err = fabs(x[i] - x_expected[i]);
        if (err > max_err) max_err = err;
    }

    if (n <= 16) {
        fprintf(stdout, "x = [\n");
        for (int i = 0; i < n; i++)
            fprintf(stdout, "  x[%d] = %12.6f   (expected %12.6f,"
                    " err = %.2e)\n",
                    i, x[i], x_expected[i], fabs(x[i] - x_expected[i]));
        fprintf(stdout, "]\n");
    }

    fprintf(stdout, "Max absolute error: %.6e  %s\n",
            max_err, max_err < 1e-9 ? "✓ PASS" : "✗ FAIL");

    free(x); free(A); free(b); free(x_expected);

    /* AICI clientul inchide politicos conexiunea prin mesajul OPR_BYE. */
    h.opID = OPR_BYE;
    writeSingleInt(sock, h, 0);
    close(sock);

    return (max_err < 1e-9) ? EXIT_SUCCESS : EXIT_FAILURE;
}
