/*
 * solve_client.c — Test client for OPR_SOLVE (T26).
 *
 * Sends a linear system Ax=b to the PCD server and prints the solution.
 * The system used by default has a known analytical solution so results
 * can be verified by eye.
 *
 * Usage:
 *   ./solve_client [-h host] [-p port] [-n size] [-w workers]
 *
 * Build:
 *   gcc solve_client.c proto.c -o solve_client -Wall -Wextra
 *   (no LAPACKE needed here — the solver runs on the server)
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

/* ------------------------------------------------------------------ */
/*  Helper: build a diagonally-dominant test system with known x      */
/*                                                                     */
/*  We choose x[i] = i+1  (so x = [1, 2, 3, ..., n]).                */
/*  Then b = A * x analytically, giving a system with a clean answer. */
/* ------------------------------------------------------------------ */

static void build_test_system(int n, double *A, double *b, double *x_expected)
{
    /* Fill A: diagonal-dominant so it's guaranteed non-singular */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j)
                A[i * n + j] = (double)(2 * n);   /* dominant diagonal */
            else
                A[i * n + j] = (double)(1 + (i + j) % 3);
        }
        x_expected[i] = (double)(i + 1);
    }

    /* b = A * x_expected */
    for (int i = 0; i < n; i++) {
        b[i] = 0.0;
        for (int j = 0; j < n; j++)
            b[i] += A[i * n + j] * x_expected[j];
    }
}

/* ------------------------------------------------------------------ */

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
    name->sin_addr   = *(struct in_addr *)hostinfo->h_addr;
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

/* ------------------------------------------------------------------ */

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

    /* ---- Allocate matrices --------------------------------------- */
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

    /* ---- Connect ------------------------------------------------- */
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in srv;
    init_sockaddr(&srv, host, (uint16_t)port);
    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(sock); return EXIT_FAILURE;
    }
    fprintf(stderr, "[client] Connected to %s:%d\n", host, port);

    /* ---- Handshake ----------------------------------------------- */
    msgHeaderType h;
    msgIntType    m;
    h.clientID = 0; h.opID = OPR_CONNECT;
    writeSingleInt(sock, h, 0);
    readSingleInt(sock, &m);
    int clientID  = m.msg;
    h.clientID    = clientID;
    fprintf(stderr, "[client] ClientID = %d\n", clientID);

    /* ---- OPR_SOLVE ----------------------------------------------- */
    h.opID = OPR_SOLVE;
    fprintf(stderr, "[client] Sending OPR_SOLVE request...\n");

    if (writeSolveRequest(sock, h, n, A, b) < 0) {
        fprintf(stderr, "[client] writeSolveRequest failed\n");
        close(sock); return EXIT_FAILURE;
    }

    int     status = 0;
    double *x      = NULL;
    if (readSolveResponse(sock, &status, &n, &x) < 0) {
        fprintf(stderr, "[client] readSolveResponse failed\n");
        close(sock); return EXIT_FAILURE;
    }

    if (status != 0) {
        fprintf(stderr, "[client] Server returned error status %d\n", status);
        close(sock); return EXIT_FAILURE;
    }

    /* ---- Print and verify solution ------------------------------- */
    fprintf(stderr, "[client] Solution received.\n");

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

    /* ---- BYE ----------------------------------------------------- */
    h.opID = OPR_BYE;
    writeSingleInt(sock, h, 0);
    close(sock);

    return (max_err < 1e-9) ? EXIT_SUCCESS : EXIT_FAILURE;
}
