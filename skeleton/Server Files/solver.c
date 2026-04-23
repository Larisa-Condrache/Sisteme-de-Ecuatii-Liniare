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
 Eliminare Gauss paralela 
 Fiecare proces copil detine o banda contigua de randuri din matricea
 augmentata [A|b] pe toata durata calculului.Dupa n pasi, copiii trimit randurile inapoi

perechiile de pipe sunt create si copiii sunt fork-ati inainte ca parintele sa inchida capetele de pipe
care apartin copilului. Inchiderea prea devreme a unui fd din copil elibereaza
 numarul descriptorului pentru reutilizare de urmatorul apel pipe(), ceea ce
 face ca urmatorul copil sa mosteneasca in tacere si apoi sa inchida fd-ul gresit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lapacke.h>
#include "solver.h"

/*
 * WRITE_ALL(fd, buf, len, err_label)
 *   Scrie exact `len` octeti din `buf` pe `fd`.
 *   La eroare sare la `err_label`.
 *   Foloseste o bucla inline — fara functie wrapper.
 */
#define WRITE_ALL(fd, buf, len, err_label)                      \
    do {                                                        \
        const char *_wa_p   = (const char *)(buf);             \
        size_t      _wa_rem = (len);                           \
        while (_wa_rem > 0) {                                   \
            ssize_t _wa_n = write((fd), _wa_p, _wa_rem);       \
            if (_wa_n < 0) {                                    \
                if (errno == EINTR) continue;                   \
                goto err_label;                                 \
            }                                                   \
            if (_wa_n == 0) goto err_label;                     \
            _wa_p   += _wa_n;                                   \
            _wa_rem -= (size_t)_wa_n;                           \
        }                                                       \
    } while (0)

/*
 * READ_ALL(fd, buf, len, err_label)
 *   Citeste exact `len` octeti de pe `fd` in `buf`.
 *   La eroare sau EOF sare la `err_label`.
 *   Foloseste o bucla inline — fara functie wrapper.
 */
#define READ_ALL(fd, buf, len, err_label)                       \
    do {                                                        \
        char   *_ra_p   = (char *)(buf);                       \
        size_t  _ra_rem = (len);                               \
        while (_ra_rem > 0) {                                   \
            ssize_t _ra_n = read((fd), _ra_p, _ra_rem);        \
            if (_ra_n < 0) {                                    \
                if (errno == EINTR) continue;                   \
                goto err_label;                                 \
            }                                                   \
            if (_ra_n == 0) goto err_label;                     \
            _ra_p   += _ra_n;                                   \
            _ra_rem -= (size_t)_ra_n;                           \
        }                                                       \
    } while (0)
static solver_result_t solve_serial(int n, const double *A, const double *b)
{
    solver_result_t res = {-2, NULL};
    double *Ac   = malloc((size_t)(n*n)*sizeof(double));
    double *xc   = malloc((size_t)n   *sizeof(double));
    lapack_int *piv = malloc((size_t)n*sizeof(lapack_int));
    if (!Ac||!xc||!piv) { free(Ac);free(xc);free(piv); return res; }

    memcpy(Ac,A,(size_t)(n*n)*sizeof(double));
    memcpy(xc,b,(size_t)n   *sizeof(double));
    lapack_int info = LAPACKE_dgesv(LAPACK_ROW_MAJOR,(lapack_int)n,1,
                                    Ac,(lapack_int)n,piv,xc,1);
    free(Ac); free(piv);
    if (info!=0) { free(xc); res.status=-1; return res; }
    res.status=0; res.x=xc;
    return res;
}

static int back_sub(int n, const double *aug, double *x)
{
    int nc=n+1;
    for (int i=n-1; i>=0; i--) {
        double s=aug[i*nc+n];
        for (int j=i+1;j<n;j++) s-=aug[i*nc+j]*x[j];
        double d=aug[i*nc+i];
        if (fabs(d)<1e-300) return -1;
        x[i]=s/d;
    }
    return 0;
}

static void worker_child(int rfd, int wfd,
                         int n, int band_start, int band_len)
{
    int     nc  = n+1;
    double *aug = malloc((size_t)(band_len*nc)*sizeof(double));
    double *piv = malloc((size_t)nc           *sizeof(double));
    if (!aug||!piv) { free(aug);free(piv); _exit(1); }

    /* Primeste banda initiala */
    READ_ALL(rfd, aug, (size_t)(band_len*nc)*sizeof(double), worker_err);

    /* Bucla de eliminare */
    for (int k=0; k<n; k++) {
        /* Trimite randul pivot in sus daca il detinem */
        if (k>=band_start && k<band_start+band_len) {
            int lk=k-band_start;
            WRITE_ALL(wfd, &aug[lk*nc], (size_t)nc*sizeof(double), worker_err);
        }
        /* Primeste pivotul broadcast */
        READ_ALL(rfd, piv, (size_t)nc*sizeof(double), worker_err);
        /* Eliminare */
        for (int li=0; li<band_len; li++) {
            int gi=band_start+li;
            if (gi<=k) continue;
            double d=piv[k];
            if (fabs(d)<1e-300) continue;
            double f=aug[li*nc+k]/d;
            for (int j=k;j<nc;j++)
                aug[li*nc+j]-=f*piv[j];
        }
    }

    /* Trimite inapoi randurile finale */
    WRITE_ALL(wfd, aug, (size_t)(band_len*nc)*sizeof(double), worker_err);

    free(aug); free(piv);
    close(rfd); close(wfd);
    _exit(0);

worker_err:
    free(aug); free(piv);
    _exit(1);
}

static int row_owner(int gi, int band, int W)
{
    int w=gi/band;
    return (w>=W)?W-1:w;
}

solver_result_t solver_solve(int n, const double *A, const double *b,
                             int num_workers)
{
    solver_result_t res={-3,NULL};

    if (num_workers<1)                  num_workers=1;
    if (num_workers>SOLVER_MAX_WORKERS) num_workers=SOLVER_MAX_WORKERS;
    if (num_workers>n)                  num_workers=n;

    if (n<SOLVER_SERIAL_THRESHOLD||num_workers==1) {
        fprintf(stderr,"[solver] n=%d → serial LAPACKE_dgesv\n",n);
        return solve_serial(n,A,b);
    }
    fprintf(stderr,"[solver] n=%d, workers=%d → parallel Gaussian\n",
            n,num_workers);

    signal(SIGPIPE, SIG_IGN);

    int W    = num_workers;
    int nc   = n+1;
    int band = n/W;

    double *aug_full = malloc((size_t)(n*nc)*sizeof(double));
    double *aug_out  = malloc((size_t)(n*nc)*sizeof(double));
    if (!aug_full||!aug_out) {
        free(aug_full);free(aug_out); res.status=-2; return res;
    }
    for (int i=0;i<n;i++) {
        memcpy(&aug_full[i*nc],&A[i*n],(size_t)n*sizeof(double));
        aug_full[i*nc+n]=b[i];
    }

    int down[SOLVER_MAX_WORKERS][2];
    int up  [SOLVER_MAX_WORKERS][2];
    pid_t pids[SOLVER_MAX_WORKERS];
    for (int w=0;w<W;w++) {
        down[w][0]=down[w][1]=-1;
        up[w][0]  =up[w][1]  =-1;
        pids[w]=-1;
    }

    /* creeaza pipe-urile inainte de orice fork  */
    for (int w=0;w<W;w++) {
        if (pipe(down[w])!=0||pipe(up[w])!=0) {
            perror("[solver] pipe"); goto fail;
        }
    }

    /*  fork pentru TOTI workerii  */
    for (int w=0;w<W;w++) {
        int bs=w*band, bl=(w==W-1)?(n-bs):band;
        pids[w]=fork();
        if (pids[w]<0) { perror("[solver] fork"); goto fail; }

        if (pids[w]==0) {
            for (int j=0;j<W;j++) {
                if (j==w) {
                    close(down[j][1]);   /* capatul de scriere al parintelui, nu ne trebuie */
                    close(up[j][0]);     /* capatul de citire al parintelui, nu ne trebuie */
                } else {
                    close(down[j][0]); close(down[j][1]);
                    close(up[j][0]);   close(up[j][1]);
                }
            }
            worker_child(down[w][0], up[w][1], n, bs, bl);
            /* nu se intoarce niciodata */
        }
    }
    for (int w=0;w<W;w++) {
        close(down[w][0]); down[w][0]=-1;
        close(up[w][1]);   up[w][1]  =-1;
    }
    for (int w=0;w<W;w++) {
        int bs=w*band, bl=(w==W-1)?(n-bs):band;
        WRITE_ALL(down[w][1], &aug_full[bs*nc],
                  (size_t)(bl*nc)*sizeof(double), fail);
    }

    {
        double *piv=malloc((size_t)nc*sizeof(double));
        if (!piv) { res.status=-2; goto fail; }

        for (int k=0;k<n;k++) {
            int ow=row_owner(k,band,W);

            /* Primeste pivotul de la detinator */
            READ_ALL(up[ow][0], piv, (size_t)nc*sizeof(double), pivot_err);
            memcpy(&aug_out[k*nc],piv,(size_t)nc*sizeof(double));

            for (int w=0;w<W;w++) {
                WRITE_ALL(down[w][1], piv, (size_t)nc*sizeof(double), pivot_err);
            }
            continue;
pivot_err:
            fprintf(stderr,"[solver] pivot exchange k=%d failed\n",k);
            free(piv); goto fail;
        }
        free(piv);
    }
    for (int w=0;w<W;w++) { close(down[w][1]); down[w][1]=-1; }

    /*  colecteaza randurile finale  */
    for (int w=0;w<W;w++) {
        int bs=w*band, bl=(w==W-1)?(n-bs):band;
        double *tmp=malloc((size_t)(bl*nc)*sizeof(double));
        if (!tmp) goto fail;
        READ_ALL(up[w][0], tmp, (size_t)(bl*nc)*sizeof(double), collect_err);
        for (int li=0;li<bl;li++) {
            int gi=bs+li;
            memcpy(&aug_out[gi*nc],&tmp[li*nc],(size_t)nc*sizeof(double));
        }
        free(tmp);
        close(up[w][0]); up[w][0]=-1;
        continue;
collect_err:
        fprintf(stderr,"[solver] collect w%d failed\n",w);
        free(tmp); goto fail;
    }

    /*  colecteaza status-urile copiilor  */
    for (int w=0;w<W;w++) {
        int st; waitpid(pids[w],&st,0); pids[w]=-1;
    }

    free(aug_full); aug_full=NULL;

    /*  substitutie inversa  */
    {
        double *x=malloc((size_t)n*sizeof(double));
        if (!x) { free(aug_out); res.status=-2; return res; }
        if (back_sub(n,aug_out,x)!=0) {
            free(aug_out);free(x); res.status=-1; return res;
        }
        free(aug_out);
        res.status=0; res.x=x;
        return res;
    }

fail:
    for (int w=0;w<W;w++) {
        if (down[w][0]!=-1) close(down[w][0]);
        if (down[w][1]!=-1) close(down[w][1]);
        if (up[w][0]  !=-1) close(up[w][0]);
        if (up[w][1]  !=-1) close(up[w][1]);
        if (pids[w]>0) { int st; waitpid(pids[w],&st,0); }
    }
    free(aug_full); free(aug_out);
    return res;
}