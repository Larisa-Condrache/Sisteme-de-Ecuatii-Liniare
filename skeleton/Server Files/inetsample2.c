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
inetsample2.c — Client de test pentru server.
Modificari fata de skeleton: host-ul si portul serverului sunt acum configurabile din CLI cu getopt) si adaugat cazul de test OPR_CONC am adaugat inchidere cu close() inainte de exit.
ex utilizare:
  ./inetclient [-h host] [-p port]
  ./inetclient --host 192.168.1.10 --port 18081
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
#include <arpa/inet.h>

#include "proto.h"

#define DEFAULT_PORT        18081
#define DEFAULT_HOST        "127.0.0.1"


static void init_sockaddr(struct sockaddr_in *name,
                          const char *hostname,
                          uint16_t port)
{
    struct hostent *hostinfo;

    name->sin_family = AF_INET;
    name->sin_port   = htons(port);
    hostinfo         = gethostbyname(hostname);
    if (hostinfo == NULL) {
        fprintf(stderr, "Unknown host: %s\n", hostname);
        exit(EXIT_FAILURE);
    }
    name->sin_addr = *(struct in_addr *)hostinfo->h_addr_list[0];
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-h host] [-p port]\n"
            "  -h, --host <addr>   Server address [default: %s]\n"
            "  -p, --port <n>      Server TCP port [default: %d]\n",
            prog, DEFAULT_HOST, DEFAULT_PORT);
}


int main(int argc, char *argv[])
{
    const char *host = DEFAULT_HOST;
    int         port = DEFAULT_PORT;
    int         sock;
    struct sockaddr_in servername;

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

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    init_sockaddr(&servername, host, (uint16_t)port);
    if (connect(sock, (struct sockaddr *)&servername,
                sizeof(servername)) < 0) {
        perror("connect");
        close(sock);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "[client] Connected to %s:%d\n", host, port);

    msgHeaderType h;
    msgIntType    m;
    msgStringType str;
    int           clientID = 0;

    h.clientID = 0;
    h.opID     = OPR_CONNECT;

    if (writeSingleInt(sock, h, 0) < 0) {
        fprintf(stderr, "[client] Failed to send connect request\n");
        close(sock); return EXIT_FAILURE;
    }
    if (readSingleInt(sock, &m) < 0) {
        fprintf(stderr, "[client] Failed to receive clientID\n");
        close(sock); return EXIT_FAILURE;
    }
    clientID  = m.msg;
    h.clientID = clientID;
    fprintf(stderr, "[client] Assigned clientID: %d\n", clientID);

    fprintf(stderr, "\n[client] === OPR_NEG ===\n");
    h.opID = OPR_NEG;
    for (int i = 0; i < 5; i++) {
        int val = -100 + i * 15;
        if (writeSingleInt(sock, h, val) < 0) break;
        if (readSingleInt(sock, &m) < 0) break;
        fprintf(stderr, "  neg(%d) = %d\n", val, m.msg);
    }

    fprintf(stderr, "\n[client] === OPR_ADD ===\n");
    h.opID = OPR_ADD;
    for (int i = 0; i < 5; i++) {
        int a = -50 + i * 10, b = 100 - i * 7;
        if (writeMultiInt(sock, h, a, b) < 0) break;
        if (readSingleInt(sock, &m) < 0) break;
        fprintf(stderr, "  add(%d, %d) = %d\n", a, b, m.msg);
    }

    fprintf(stderr, "\n[client] === OPR_ECHO ===\n");
    h.opID = OPR_ECHO;
    {
        const char *outgoing = "Hello PCD server!";
        if (writeSingleString(sock, h, (char *)outgoing) >= 0 &&
            readSingleString(sock, &str) >= 0) {
            fprintf(stderr, "  echo(\"%s\") = \"%s\"\n", outgoing, str.msg);
            free(str.msg);
        }
    }

    fprintf(stderr, "\n[client] === OPR_CONC ===\n");
    h.opID = OPR_CONC;
    {
        const char *s1 = "Hello";
        const char *s2 = "World";
        /* Trimite doua siruri pe rand */
        if (writeSingleString(sock, h, (char *)s1) >= 0 &&
            writeSingleString(sock, h, (char *)s2) >= 0 &&
            readSingleString(sock, &str) >= 0) {
            fprintf(stderr, "  conc(\"%s\", \"%s\") = \"%s\"\n",
                    s1, s2, str.msg);
            free(str.msg);
        }
    }

    fprintf(stderr, "\n[client] === OPR_BYE ===\n");
    h.opID = OPR_BYE;
    writeSingleInt(sock, h, 0); /* serverul se inchide */

    close(sock);
    fprintf(stderr, "[client] Done.\n");
    return EXIT_SUCCESS;
}
