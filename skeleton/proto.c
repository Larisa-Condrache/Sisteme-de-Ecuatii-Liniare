/*
<<<<<<< HEAD
 Modificari fata de skeleton: am eliminat apelul de debug write(2, str, strSize) din writeSingleString(), 
=======
 * proto.c — Binary protocol helpers (read/write message types).
 *
 * Changes from skeleton:
 *   • Removed #include <ncurses.h> (not used here).
 *   • Removed bare write(2, str, strSize) debug call in writeSingleString().
 *   • All debug output now goes through fprintf(stderr, …) and is guarded
 *     by the DEBUG macro so it can be stripped for release builds.
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#include "proto.h"

<<<<<<< HEAD
=======
/* Define DEBUG to enable verbose recv/send tracing. */
/* #define DEBUG */

/* ------------------------------------------------------------------ */

>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
msgHeaderType peekMsgHeader(int sock)
{
    ssize_t       nb;
    msgHeaderType h;

    h.msgSize  = htonl(sizeof(h));
    h.clientID = 0;
    h.opID     = 0;

    nb = recv(sock, &h, sizeof(h), MSG_PEEK | MSG_WAITALL);

<<<<<<< HEAD
    /* Conversie din ordinea de octeti de retea */
=======
    /* Convert from network byte order */
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
    h.msgSize  = ntohl(h.msgSize);
    h.clientID = ntohl(h.clientID);
    h.opID     = ntohl(h.opID);

    if (nb == -1) {
        fprintf(stderr, "[proto] peekMsgHeader: recv error: %s\n",
                strerror(errno));
        h.opID = h.clientID = -1;
    } else if (nb == 0) {
<<<<<<< HEAD
        /* Capatul remote a inchis conexiunea */
=======
        /* Remote end closed the connection */
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
        h.opID = h.clientID = OPR_BYE;
    }

#ifdef DEBUG
    fprintf(stderr, "[proto] peekMsgHeader: size=%d clientID=%d opID=%d"
            " (nb=%zd)\n", h.msgSize, h.clientID, h.opID, nb);
#endif

    return h;
}

<<<<<<< HEAD
=======
/* ------------------------------------------------------------------ */

>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
int readSingleInt(int sock, msgIntType *m)
{
    ssize_t         nb;
    singleIntMsgType s;

    nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
    if (nb <= 0) {
        m->msg = -1;
        return -1;
    }
    m->msg = ntohl(s.i.msg);
    return (int)nb;
}

int writeSingleInt(int sock, msgHeaderType h, int i)
{
    ssize_t         nb;
    singleIntMsgType s;

    s.header.clientID = htonl(h.clientID);
    s.header.opID     = htonl(h.opID);
    s.header.msgSize  = htonl(sizeof(s));
    s.i.msg           = htonl(i);

    nb = send(sock, &s, sizeof(s), 0);
    if (nb <= 0)
        return -1;
    return (int)nb;
}

<<<<<<< HEAD
=======
/* ------------------------------------------------------------------ */

>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
int readMultiInt(int sock, msgIntType *m1, msgIntType *m2)
{
    ssize_t        nb;
    multiIntMsgType s;

    nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
    if (nb <= 0) {
        m1->msg = m2->msg = -1;
        return -1;
    }
    m1->msg = ntohl(s.i.msg1);
    m2->msg = ntohl(s.i.msg2);
    return (int)nb;
}

int writeMultiInt(int sock, msgHeaderType h, int i1, int i2)
{
    ssize_t        nb;
    multiIntMsgType s;

    s.header.clientID = htonl(h.clientID);
    s.header.opID     = htonl(h.opID);
    s.header.msgSize  = htonl(sizeof(s));
    s.i.msg1          = htonl(i1);
    s.i.msg2          = htonl(i2);

    nb = send(sock, &s, sizeof(s), 0);
    if (nb <= 0)
        return -1;
    return (int)nb;
}

<<<<<<< HEAD
=======
/* ------------------------------------------------------------------ */
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619

int readSingleString(int sock, msgStringType *str)
{
    ssize_t    nb;
    msgIntType m;

<<<<<<< HEAD
    /*  primim lungimea sirului ca mesaj SingleInt */
=======
    /* First receive the string length as a SingleInt message */
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
    if (readSingleInt(sock, &m) < 0)
        return -1;

    if (m.msg < 0) {
        fprintf(stderr, "[proto] readSingleString: invalid string length %d\n",
                m.msg);
        return -1;
    }

#ifdef DEBUG
    fprintf(stderr, "[proto] readSingleString: expecting %d bytes\n", m.msg);
#endif

    str->msg = (char *)malloc((size_t)m.msg + 1);
    if (str->msg == NULL) {
        fprintf(stderr, "[proto] readSingleString: malloc failed\n");
        return -1;
    }

    nb = recv(sock, str->msg, (size_t)m.msg, MSG_WAITALL);
    if (nb < 0) {
        free(str->msg);
        str->msg = NULL;
        return -1;
    }

    str->msg[m.msg] = '\0';

#ifdef DEBUG
    fprintf(stderr, "[proto] readSingleString: received \"%s\" (%zd bytes)\n",
            str->msg, nb);
#endif

    return (int)nb;
}

int writeSingleString(int sock, msgHeaderType h, char *str)
{
    ssize_t nb;
    int     strSize = (int)strlen(str);

<<<<<<< HEAD
    /* Trimitem lungimea sirului */
=======
    /* Send the string length first */
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
    nb = writeSingleInt(sock, h, strSize);
    if (nb < 0)
        return -1;

#ifdef DEBUG
    fprintf(stderr, "[proto] writeSingleString: sent length %d\n", strSize);
#endif

<<<<<<< HEAD
=======
    /* Then send the raw string bytes (no null terminator) */
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
    nb = send(sock, str, (size_t)strSize, 0);
    if (nb < 0) {
        fprintf(stderr, "[proto] writeSingleString: send error: %s\n",
                strerror(errno));
        return -1;
    }

#ifdef DEBUG
    fprintf(stderr, "[proto] writeSingleString: sent \"%s\" (%zd bytes)\n",
            str, nb);
#endif

    return (int)nb;
}

<<<<<<< HEAD
#include <stdint.h>

/* writeSolveRequest() — clientul trimite n, A matrice, b (n). Dispunere pe fir: [solveRequestHeaderType] [n*n double-uri pentru A] [n double-uri pentru b], toate valorile double sunt trimise in ordinea de octeti nativa 
=======
/* ------------------------------------------------------------------ */
/*  OPR_SOLVE wire helpers                                             */
/* ------------------------------------------------------------------ */

#include <stdint.h>

/*
 * writeSolveRequest() — client sends n, A (n×n row-major), b (n).
 *
 * Layout on the wire:
 *   [solveRequestHeaderType] [n*n doubles for A] [n doubles for b]
 *
 * All doubles are sent in native byte order — both sides are expected
 * to run on the same or compatible architectures (IEEE 754 little-
 * endian in practice on x86/ARM64 Linux).
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
 */
int writeSolveRequest(int sock, msgHeaderType h,
                      int n, const double *A, const double *b)
{
    solveRequestHeaderType rh;
    ssize_t nb;

    rh.hdr.clientID = htonl((uint32_t)h.clientID);
    rh.hdr.opID     = htonl(OPR_SOLVE);
    rh.hdr.msgSize  = htonl((uint32_t)sizeof(rh));
    rh.n            = htonl((uint32_t)n);

    nb = send(sock, &rh, sizeof(rh), 0);
    if (nb <= 0) return -1;

    nb = send(sock, A, (size_t)(n * n) * sizeof(double), 0);
    if (nb <= 0) return -1;

    nb = send(sock, b, (size_t)n * sizeof(double), 0);
    if (nb <= 0) return -1;

    return 0;
}

<<<<<<< HEAD
/* readSolveRequest() — serverul citeste n, aloca A si b si le populeaza.
=======
/*
 * readSolveRequest() — server reads n, allocates A and b, fills them.
 * Caller must free(*A) and free(*b).
 * The header was already peeked; this call consumes the full message.
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
 */
int readSolveRequest(int sock, int *n, double **A, double **b)
{
    solveRequestHeaderType rh;
    ssize_t nb;

    nb = recv(sock, &rh, sizeof(rh), MSG_WAITALL);
    if (nb <= 0) return -1;

    *n = (int)ntohl((uint32_t)rh.n);
    if (*n <= 0 || *n > 4096) return -1;

    *A = (double *)malloc((size_t)(*n) * (size_t)(*n) * sizeof(double));
    *b = (double *)malloc((size_t)(*n) * sizeof(double));
    if (!*A || !*b) { free(*A); free(*b); *A = NULL; *b = NULL; return -1; }

    nb = recv(sock, *A, (size_t)(*n * *n) * sizeof(double), MSG_WAITALL);
    if (nb <= 0) { free(*A); free(*b); return -1; }

    nb = recv(sock, *b, (size_t)(*n) * sizeof(double), MSG_WAITALL);
    if (nb <= 0) { free(*A); free(*b); return -1; }

    return 0;
}

<<<<<<< HEAD
/* writeSolveResponse() — serverul trimite status si solutia x.
=======
/*
 * writeSolveResponse() — server sends status and (if ok) solution x.
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
 */
int writeSolveResponse(int sock, msgHeaderType h,
                       int status, int n, const double *x)
{
    solveResponseHeaderType rh;
    ssize_t nb;

    rh.hdr.clientID = htonl((uint32_t)h.clientID);
    rh.hdr.opID     = htonl(OPR_SOLVE);
    rh.hdr.msgSize  = htonl((uint32_t)sizeof(rh));
    rh.status       = htonl((uint32_t)status);
<<<<<<< HEAD
    rh.n            = htonl((uint32_t)n);
=======
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619

    nb = send(sock, &rh, sizeof(rh), 0);
    if (nb <= 0) return -1;

    if (status == 0 && x != NULL) {
        nb = send(sock, x, (size_t)n * sizeof(double), 0);
        if (nb <= 0) return -1;
    }

    return 0;
}

/*
<<<<<<< HEAD
readSolveResponse() — clientul citeste status si aloca x.
=======
 * readSolveResponse() — client reads status and (if ok) allocates x.
 * Caller must free(*x) when status == 0.
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
 */
int readSolveResponse(int sock, int *status, int *n, double **x)
{
    solveResponseHeaderType rh;
    ssize_t nb;

    nb = recv(sock, &rh, sizeof(rh), MSG_WAITALL);
    if (nb <= 0) return -1;

    *status = (int)ntohl((uint32_t)rh.status);
<<<<<<< HEAD
    *n      = (int)ntohl((uint32_t)rh.n);
=======
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619

    if (*status != 0 || *n <= 0) {
        *x = NULL;
        return 0;
    }

    *x = (double *)malloc((size_t)(*n) * sizeof(double));
    if (!*x) return -1;

    nb = recv(sock, *x, (size_t)(*n) * sizeof(double), MSG_WAITALL);
    if (nb <= 0) { free(*x); *x = NULL; return -1; }

    return 0;
}
