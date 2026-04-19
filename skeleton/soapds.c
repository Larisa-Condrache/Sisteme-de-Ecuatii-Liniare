/*
 * soapds.c — SOAP/gsoap server component.
 *
 * Changes from skeleton:
 *   • Removed #include <ncurses.h> (not available / not needed).
 *   • Removed commented-out ncurses window code.
 *   • create_client_id() now resolved via inetds2.c (external linkage).
 *   • Added g_cfg.log_level guard on diagnostic output.
 *   • Reformatted for consistency.
 *
 * Build dependency: libgsoap  (sudo apt install gsoap libgsoap-dev)
 * The generated files soapC.c / soapServer.c / soapH.h / soapStub.h
 * are produced by:  soapcpp2 -S -c -x sclient.h
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "SampleServices.nsmap"
#include "soapH.h"
#include "config.h"

/* Defined in inetds2.c */
extern int create_client_id(void);

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static char *do_client_concat(struct soap *soap,
                              const char *o1, const char *o2)
{
    int   bsize = (int)(strlen(o1) + strlen(o2) + 2);
    char *b     = (char *)soap_malloc(soap, (size_t)bsize);
    if (b == NULL)
        return NULL;
    snprintf(b, (size_t)bsize, "%s %s", o2, o1);
    return b;
}

/* ------------------------------------------------------------------ */
/*  Thread entry-point                                                 */
/* ------------------------------------------------------------------ */

void *soap_main(void *args)
{
    struct soap soap;
    int         msd, csd;
    int         port        = *(int *)(args);
    int         reuseAddrON = 1;

    soap_init(&soap);
    soap.bind_flags = SO_REUSEADDR;

    msd = soap_bind(&soap, "127.0.0.1", port, 100);
    if (!soap_valid_socket(msd)) {
        soap_print_fault(&soap, stderr);
        pthread_exit(NULL);
    }

    setsockopt(msd, SOL_SOCKET, SO_REUSEADDR,
               &reuseAddrON, sizeof(reuseAddrON));

    if (g_cfg.log_level >= 1)
        fprintf(stderr, "[soap] Listening on port %d\n", port);

    for (;;) {
        csd = soap_accept(&soap);
        if (csd < 0) {
            soap_print_fault(&soap, stderr);
            break;
        }

        if (soap_serve(&soap) != SOAP_OK)
            soap_print_fault(&soap, stderr);

        soap_destroy(&soap);
        soap_end(&soap);
    }

    soap_done(&soap);
    pthread_exit(NULL);
}

/* ------------------------------------------------------------------ */
/*  gsoap service operation implementations                           */
/* ------------------------------------------------------------------ */

int ns__bye(struct soap *s,
            struct byeStruct rq,
            struct ns__byeResponse *rsp)
{
    (void)s; (void)rq; (void)rsp; /* suppress unused-parameter warnings */
    return SOAP_OK;
}

int ns__connect(struct soap *s, long *rsp)
{
    (void)s;
    *rsp = (long)create_client_id();
    return SOAP_OK;
}

int ns__echo(struct soap *s, char *rq, char **rsp)
{
    *rsp = do_client_concat(s, ":: echo ::", rq);
    return SOAP_OK;
}

int ns__concat(struct soap *s, struct concatStruct rq, char **rsp)
{
    *rsp = do_client_concat(s, rq.op1, rq.op2);
    return SOAP_OK;
}

int ns__adder(struct soap *s, struct addStruct rq, long *rsp)
{
    (void)s;
    *rsp = rq.op1 + rq.op2;
    return SOAP_OK;
}
