/*
 * equation_client.c — Client pentru rezolvarea ecuațiilor de tip ax + b = c
 * Traduce ecuația în format Ax = b și o trimite la serverul PCD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "proto.h"

void parse_equation(const char *str, double *a, double *b, double *c) {
    // Încercăm să citim formatul "ax+b=c"
    // Exemplu: "2x+4=6" -> a=2, b=4, c=6
    if (sscanf(str, "%lfx+%lf=%lf", a, b, c) == 3) return;

    // Încercăm formatul "x+b=c" (unde a este implicit 1)
    if (sscanf(str, "x+%lf=%lf", b, c) == 2) {
        *a = 1.0;
        return;
    }

    fprintf(stderr, "Format invalid! Foloseste: ax+b=c (ex: 2x+4=6)\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Utilizare: %s \"ecuatie\"\nExemplu: %s \"2x+4=6\"\n", argv[0], argv[0]);
        return 1;
    }

    double a, b, c;
    parse_equation(argv[1], &a, &b, &c);

    // Transformăm ax + b = c  în  Ax = B
    // n = 1
    // Matricea A = [a]
    // Vectorul B = [c - b]
    int n = 1;
    double A_mat[1] = { a };
    double B_vec[1] = { c - b };

    printf("[client] Rezolvam: %.2fx + %.2f = %.2f\n", a, b, c);
    printf("[client] Traducere: %.2fx = %.2f\n", a, c - b);

    // ---- Conectare la server ----
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv;
    struct hostent *hostinfo = gethostbyname("127.0.0.1");
    srv.sin_family = AF_INET;
    srv.sin_port = htons(18081);
    srv.sin_addr = *(struct in_addr *)hostinfo->h_addr_list[0];

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        return 1;
    }

    // ---- Protocol Handshake ----
    msgHeaderType h;
    msgIntType m;
    h.clientID = 0; h.opID = OPR_CONNECT;
    writeSingleInt(sock, h, 0);
    readSingleInt(sock, &m);
    h.clientID = m.msg;

    // ---- Trimitere OPR_SOLVE ----
    h.opID = OPR_SOLVE;
    if (writeSolveRequest(sock, h, n, A_mat, B_vec) < 0) {
        printf("Eroare la trimitere!\n");
        return 1;
    }

    // ---- Primire Rezultat ----
    int status, n_out;
    double *x_result = NULL;
    if (readSolveResponse(sock, &status, &n_out, &x_result) < 0 || status != 0) {
        printf("Serverul a raportat eroare!\n");
        return 1;
    }

    printf("\n[REZULTAT] x = %.4f\n", x_result[0]);

    free(x_result);
    close(sock);
    return 0;
}
