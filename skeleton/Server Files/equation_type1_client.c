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
 * equation_type1_client.c — Client pentru ecuații de ordin 1, 2 sau 3.
 *
 * Ordin 1 (1 ecuație, 1 necunoscută x):
 *   ./equation_type1_client -n 1 "2x+4=6"
 *   Formate: ax+b=c  |  ax-b=c  |  ax=c  |  x+b=c  |  x-b=c  |  x=c
 *
 * Ordin 2 (2 ecuații, 2 necunoscute x, y):
 *   ./equation_type1_client -n 2 "2x+3y=7" "x+2y=4"
 *   Format: ax+by=c  |  ax-by=c  (coeficienții pot fi negativi)
 *
 * Ordin 3 (3 ecuații, 3 necunoscute x, y, z):
 *   ./equation_type1_client -n 3 "x+y+z=6" "2x-y+z=3" "x+2y-z=2"
 *   Format: ax+by+cz=d  (toate combinațiile de semne între termeni)
 *
 * Build:
 *   gcc equation_type1_client.c proto.c -o equation_type1_client -Wall -Wextra
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

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 18081

static const char *var_name[] = { "x", "y", "z" };

/* ------------------------------------------------------------------ */
/*  Parsere per ordin                                                  */
/* ------------------------------------------------------------------ */

/* Ordin 1: ax + b = c  →  row[0]*x = rhs */
static void parse_eq1(const char *str, double row[1], double *rhs)
{
    double a, b, c;

    if (sscanf(str, "%lfx+%lf=%lf", &a, &b, &c) == 3) { row[0] = a; *rhs = c - b; return; }
    if (sscanf(str, "%lfx-%lf=%lf", &a, &b, &c) == 3) { row[0] = a; *rhs = c + b; return; }
    if (sscanf(str, "%lfx=%lf",     &a, &c)     == 2) { row[0] = a; *rhs = c;     return; }
    if (sscanf(str, "x+%lf=%lf",   &b, &c)     == 2) { row[0] = 1.0; *rhs = c - b; return; }
    if (sscanf(str, "x-%lf=%lf",   &b, &c)     == 2) { row[0] = 1.0; *rhs = c + b; return; }
    if (sscanf(str, "x=%lf",       &c)          == 1) { row[0] = 1.0; *rhs = c;     return; }

    fprintf(stderr,
        "Format invalid pentru ecuatie de ordin 1: \"%s\"\n"
        "Formate acceptate: ax+b=c | ax-b=c | ax=c | x+b=c | x-b=c | x=c\n", str);
    exit(EXIT_FAILURE);
}

/* Ordin 2: ax + by = c  →  row[0]*x + row[1]*y = rhs */
static void parse_eq2(const char *str, double row[2], double *rhs)
{
    double a, b, c;

    if (sscanf(str, "%lfx+%lfy=%lf", &a, &b, &c) == 3) { row[0] = a;    row[1] =  b; *rhs = c; return; }
    if (sscanf(str, "%lfx-%lfy=%lf", &a, &b, &c) == 3) { row[0] = a;    row[1] = -b; *rhs = c; return; }
    if (sscanf(str,  "x+%lfy=%lf",      &b, &c) == 2) { row[0] = 1.0;  row[1] =  b; *rhs = c; return; }
    if (sscanf(str,  "x-%lfy=%lf",      &b, &c) == 2) { row[0] = 1.0;  row[1] = -b; *rhs = c; return; }
    if (sscanf(str, "-x+%lfy=%lf",      &b, &c) == 2) { row[0] = -1.0; row[1] =  b; *rhs = c; return; }
    if (sscanf(str, "-x-%lfy=%lf",      &b, &c) == 2) { row[0] = -1.0; row[1] = -b; *rhs = c; return; }

    fprintf(stderr,
        "Format invalid pentru ecuatie de ordin 2: \"%s\"\n"
        "Formate acceptate: ax+by=c | ax-by=c | x+by=c | x-by=c\n"
        "  (coeficient negativ la x: ex. -2x+3y=5 sau -x+3y=5)\n", str);
    exit(EXIT_FAILURE);
}

/* Ordin 3: ax + by + cz = d  →  row[0]*x + row[1]*y + row[2]*z = rhs
 * Suporta coeficienti impliciti (1/-1) pentru oricare dintre x, y, z    */
static void parse_eq3(const char *str, double row[3], double *rhs)
{
    double a, b, c, d;

    /* coeficient explicit la x */
    if (sscanf(str, "%lfx+%lfy+%lfz=%lf", &a, &b, &c, &d) == 4) { row[0]=a;    row[1]= b;    row[2]= c;    *rhs=d; return; }
    if (sscanf(str, "%lfx+%lfy-%lfz=%lf", &a, &b, &c, &d) == 4) { row[0]=a;    row[1]= b;    row[2]=-c;    *rhs=d; return; }
    if (sscanf(str, "%lfx-%lfy+%lfz=%lf", &a, &b, &c, &d) == 4) { row[0]=a;    row[1]=-b;    row[2]= c;    *rhs=d; return; }
    if (sscanf(str, "%lfx-%lfy-%lfz=%lf", &a, &b, &c, &d) == 4) { row[0]=a;    row[1]=-b;    row[2]=-c;    *rhs=d; return; }
    /* coeficient implicit la x (1) */
    if (sscanf(str,  "x+%lfy+%lfz=%lf",      &b, &c, &d) == 3) { row[0]=1.0;  row[1]= b;    row[2]= c;    *rhs=d; return; }
    if (sscanf(str,  "x+%lfy-%lfz=%lf",      &b, &c, &d) == 3) { row[0]=1.0;  row[1]= b;    row[2]=-c;    *rhs=d; return; }
    if (sscanf(str,  "x-%lfy+%lfz=%lf",      &b, &c, &d) == 3) { row[0]=1.0;  row[1]=-b;    row[2]= c;    *rhs=d; return; }
    if (sscanf(str,  "x-%lfy-%lfz=%lf",      &b, &c, &d) == 3) { row[0]=1.0;  row[1]=-b;    row[2]=-c;    *rhs=d; return; }
    /* coeficient implicit la x (-1) */
    if (sscanf(str, "-x+%lfy+%lfz=%lf",      &b, &c, &d) == 3) { row[0]=-1.0; row[1]= b;    row[2]= c;    *rhs=d; return; }
    if (sscanf(str, "-x+%lfy-%lfz=%lf",      &b, &c, &d) == 3) { row[0]=-1.0; row[1]= b;    row[2]=-c;    *rhs=d; return; }
    if (sscanf(str, "-x-%lfy+%lfz=%lf",      &b, &c, &d) == 3) { row[0]=-1.0; row[1]=-b;    row[2]= c;    *rhs=d; return; }
    if (sscanf(str, "-x-%lfy-%lfz=%lf",      &b, &c, &d) == 3) { row[0]=-1.0; row[1]=-b;    row[2]=-c;    *rhs=d; return; }

    fprintf(stderr,
        "Format invalid pentru ecuatie de ordin 3: \"%s\"\n"
        "Format acceptat: ax+by+cz=d (toate combinatiile de semne intre termeni)\n"
        "  coeficientii 1/-1 pot fi omisi: ex. x+2y-z=5, -x+y+3z=2\n", str);
    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------ */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Utilizare: %s -n <1|2|3> [-h host] [-p port] \"ec1\" [\"ec2\" \"ec3\"]\n\n"
        "  Ordin 1:  %s -n 1 \"2x+4=6\"\n"
        "  Ordin 2:  %s -n 2 \"2x+3y=7\" \"x+2y=4\"\n"
        "  Ordin 3:  %s -n 3 \"x+y+z=6\" \"2x-y+z=3\" \"x+2y-z=2\"\n",
        prog, prog, prog, prog);
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    const char *host = DEFAULT_HOST;
    int         port = DEFAULT_PORT;
    int         n    = 0;

    int c;
    while ((c = getopt(argc, argv, "n:h:p:")) != -1) {
        switch (c) {
        case 'n': n    = atoi(optarg); break;
        case 'h': host = optarg;       break;
        case 'p': port = atoi(optarg); break;
        default:  print_usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (n < 1 || n > 3) {
        fprintf(stderr, "Eroare: -n trebuie sa fie 1, 2 sau 3.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc - optind < n) {
        fprintf(stderr, "Eroare: sunt necesare exact %d ecuatie(i) ca argumente.\n", n);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* ---- Construire matrice A si vector b ---- */
    double *A_mat = (double *)calloc((size_t)(n * n), sizeof(double));
    double *B_vec = (double *)calloc((size_t)n,       sizeof(double));
    if (!A_mat || !B_vec) {
        fprintf(stderr, "malloc failed\n");
        return EXIT_FAILURE;
    }

    printf("[client] Sistem de ordin %d:\n", n);
    for (int i = 0; i < n; i++) {
        double row[3] = {0.0, 0.0, 0.0};
        double rhs    = 0.0;

        switch (n) {
        case 1: parse_eq1(argv[optind + i], row, &rhs); break;
        case 2: parse_eq2(argv[optind + i], row, &rhs); break;
        case 3: parse_eq3(argv[optind + i], row, &rhs); break;
        }

        for (int j = 0; j < n; j++)
            A_mat[i * n + j] = row[j];
        B_vec[i] = rhs;

        printf("  Ec %d: ", i + 1);
        for (int j = 0; j < n; j++) {
            if (j == 0) printf("%.4g%s",   row[j], var_name[j]);
            else        printf(" %+.4g%s", row[j], var_name[j]);
        }
        printf(" = %.4g\n", rhs);
    }

    /* ---- Conectare la server ---- */
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    struct hostent *hostinfo = gethostbyname(host);
    if (!hostinfo) {
        fprintf(stderr, "Host necunoscut: %s\n", host);
        close(sock); return EXIT_FAILURE;
    }

    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port   = htons((uint16_t)port);
    srv.sin_addr   = *(struct in_addr *)hostinfo->h_addr_list[0];

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(sock); return EXIT_FAILURE;
    }
    printf("[client] Conectat la %s:%d\n", host, port);

    /* ---- Handshake ---- */
    msgHeaderType h;
    msgIntType    mi;
    h.clientID = 0; h.opID = OPR_CONNECT;
    writeSingleInt(sock, h, 0);
    readSingleInt(sock, &mi);
    h.clientID = mi.msg;

    /* ---- OPR_SOLVE ---- */
    h.opID = OPR_SOLVE;
    if (writeSolveRequest(sock, h, n, A_mat, B_vec) < 0) {
        fprintf(stderr, "Eroare la trimitere!\n");
        close(sock); return EXIT_FAILURE;
    }

    int     status = 0, n_out = 0;
    double *x_result = NULL;
    if (readSolveResponse(sock, &status, &n_out, &x_result) < 0 || status != 0) {
        fprintf(stderr, "Serverul a raportat eroare (status=%d)!\n", status);
        close(sock); return EXIT_FAILURE;
    }

    /* ---- Afisare rezultat ---- */
    printf("\n[REZULTAT]\n");
    for (int i = 0; i < n_out; i++)
        printf("  %s = %.6f\n", var_name[i], x_result[i]);

    free(x_result);
    free(A_mat);
    free(B_vec);

    /* ---- BYE ---- */
    h.opID = OPR_BYE;
    writeSingleInt(sock, h, 0);
    close(sock);

    return EXIT_SUCCESS;
}
