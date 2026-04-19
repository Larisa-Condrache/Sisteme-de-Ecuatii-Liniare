/*
 * solver.c — Parallel Gaussian elimination with row-band distribution.
 *
 * Each child process owns a contiguous band of rows of the augmented
 * matrix [A|b] for the entire computation.  Communication per step k:
 *   1. Pivot-row owner → parent (up-pipe): sends row k (n+1 doubles)
 *   2. Parent → all workers (down-pipe broadcast): sends row k
 *   3. Each worker eliminates column k from its rows below k
 * After n steps, children send their rows back; parent back-substitutes.
 *
 * Key correctness detail: ALL pipe pairs are created and ALL children
 * are forked BEFORE the parent closes any child-side pipe ends.
 * Closing a child-side fd early frees the file-descriptor number for
 * reuse by the next pipe() call, causing the next child to silently
 * inherit and then close the wrong fd (classic Unix fd-reuse bug).
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

/* ------------------------------------------------------------------ */
/*  I/O helpers                                                        */
/* ------------------------------------------------------------------ */
static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n<0) { if (errno==EINTR) continue; return -1; }
        if (n==0) return -1;
        p+=n; len-=(size_t)n;
    }
    return 0;
}
static int read_all(int fd, void *buf, size_t len)
{
    char *p = buf;
    while (len) {
        ssize_t n = read(fd, p, len);
        if (n<0) { if (errno==EINTR) continue; return -1; }
        if (n==0) return -1;
        p+=n; len-=(size_t)n;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Serial path: LAPACKE_dgesv on a working copy                      */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/*  Back-substitution                                                  */
/* ------------------------------------------------------------------ */
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

/* ------------------------------------------------------------------ */
/*  Worker child                                                       */
/* ------------------------------------------------------------------ */
static void worker_child(int rfd, int wfd,
                         int n, int band_start, int band_len)
{
    int     nc  = n+1;
    double *aug = malloc((size_t)(band_len*nc)*sizeof(double));
    double *piv = malloc((size_t)nc           *sizeof(double));
    if (!aug||!piv) { free(aug);free(piv); _exit(1); }

    /* Receive initial band */
    if (read_all(rfd,aug,(size_t)(band_len*nc)*sizeof(double))!=0)
        { free(aug);free(piv); _exit(1); }

    /* Elimination loop */
    for (int k=0; k<n; k++) {
        /* Send pivot row up if we own it */
        if (k>=band_start && k<band_start+band_len) {
            int lk=k-band_start;
            if (write_all(wfd,&aug[lk*nc],(size_t)nc*sizeof(double))!=0)
                { free(aug);free(piv); _exit(1); }
        }
        /* Receive broadcast pivot */
        if (read_all(rfd,piv,(size_t)nc*sizeof(double))!=0)
            { free(aug);free(piv); _exit(1); }
        /* Eliminate */
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

    /* Send final rows back */
    if (write_all(wfd,aug,(size_t)(band_len*nc)*sizeof(double))!=0)
        { free(aug);free(piv); _exit(1); }

    free(aug); free(piv);
    close(rfd); close(wfd);
    _exit(0);
}

/* ------------------------------------------------------------------ */
/*  Which worker owns global row gi?                                  */
/* ------------------------------------------------------------------ */
static int row_owner(int gi, int band, int W)
{
    int w=gi/band;
    return (w>=W)?W-1:w;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */
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

    /* Ignore SIGPIPE so write() returns EPIPE instead of killing us */
    signal(SIGPIPE, SIG_IGN);

    int W    = num_workers;
    int nc   = n+1;
    int band = n/W;

    /* Augmented matrix [A|b] */
    double *aug_full = malloc((size_t)(n*nc)*sizeof(double));
    double *aug_out  = malloc((size_t)(n*nc)*sizeof(double));
    if (!aug_full||!aug_out) {
        free(aug_full);free(aug_out); res.status=-2; return res;
    }
    for (int i=0;i<n;i++) {
        memcpy(&aug_full[i*nc],&A[i*n],(size_t)n*sizeof(double));
        aug_full[i*nc+n]=b[i];
    }

    /* Pipe arrays — initialise to -1 so cleanup knows what to close */
    int down[SOLVER_MAX_WORKERS][2];
    int up  [SOLVER_MAX_WORKERS][2];
    pid_t pids[SOLVER_MAX_WORKERS];
    for (int w=0;w<W;w++) {
        down[w][0]=down[w][1]=-1;
        up[w][0]  =up[w][1]  =-1;
        pids[w]=-1;
    }

    /* ── PHASE 1: create ALL pipes before any fork ─────────────────── */
    for (int w=0;w<W;w++) {
        if (pipe(down[w])!=0||pipe(up[w])!=0) {
            perror("[solver] pipe"); goto fail;
        }
    }

    /* ── PHASE 2: fork ALL workers ──────────────────────────────────── */
    for (int w=0;w<W;w++) {
        int bs=w*band, bl=(w==W-1)?(n-bs):band;
        pids[w]=fork();
        if (pids[w]<0) { perror("[solver] fork"); goto fail; }

        if (pids[w]==0) {
            /* Child: close every pipe end except our own rfd/wfd */
            for (int j=0;j<W;j++) {
                if (j==w) {
                    close(down[j][1]);   /* parent write-end, child doesn't need */
                    close(up[j][0]);     /* parent read-end,  child doesn't need */
                } else {
                    close(down[j][0]); close(down[j][1]);
                    close(up[j][0]);   close(up[j][1]);
                }
            }
            worker_child(down[w][0], up[w][1], n, bs, bl);
            /* never returns */
        }
    }

    /* ── PHASE 3: parent closes child-side ends (AFTER all forks) ──── */
    for (int w=0;w<W;w++) {
        close(down[w][0]); down[w][0]=-1;
        close(up[w][1]);   up[w][1]  =-1;
    }

    /* ── PHASE 4: send initial bands ───────────────────────────────── */
    for (int w=0;w<W;w++) {
        int bs=w*band, bl=(w==W-1)?(n-bs):band;
        if (write_all(down[w][1],&aug_full[bs*nc],
                      (size_t)(bl*nc)*sizeof(double))!=0) {
            fprintf(stderr,"[solver] init send w%d failed\n",w);
            goto fail;
        }
    }

    /* ── PHASE 5: elimination loop ──────────────────────────────────── */
    {
        double *piv=malloc((size_t)nc*sizeof(double));
        if (!piv) { res.status=-2; goto fail; }

        for (int k=0;k<n;k++) {
            int ow=row_owner(k,band,W);

            /* Receive pivot from owner */
            if (read_all(up[ow][0],piv,(size_t)nc*sizeof(double))!=0) {
                fprintf(stderr,"[solver] pivot recv w%d k=%d failed\n",ow,k);
                free(piv); goto fail;
            }
            memcpy(&aug_out[k*nc],piv,(size_t)nc*sizeof(double));

            /* Broadcast to all workers */
            for (int w=0;w<W;w++) {
                if (write_all(down[w][1],piv,(size_t)nc*sizeof(double))!=0) {
                    fprintf(stderr,"[solver] broadcast w%d k=%d failed\n",w,k);
                    free(piv); goto fail;
                }
            }
        }
        free(piv);
    }

    /* ── PHASE 6: close down-pipes → children finish ────────────────── */
    for (int w=0;w<W;w++) { close(down[w][1]); down[w][1]=-1; }

    /* ── PHASE 7: collect final rows ───────────────────────────────── */
    for (int w=0;w<W;w++) {
        int bs=w*band, bl=(w==W-1)?(n-bs):band;
        double *tmp=malloc((size_t)(bl*nc)*sizeof(double));
        if (!tmp) goto fail;
        if (read_all(up[w][0],tmp,(size_t)(bl*nc)*sizeof(double))!=0) {
            fprintf(stderr,"[solver] collect w%d failed\n",w);
            free(tmp); goto fail;
        }
        for (int li=0;li<bl;li++) {
            int gi=bs+li;
            memcpy(&aug_out[gi*nc],&tmp[li*nc],(size_t)nc*sizeof(double));
        }
        free(tmp);
        close(up[w][0]); up[w][0]=-1;
    }

    /* ── PHASE 8: reap children ─────────────────────────────────────── */
    for (int w=0;w<W;w++) {
        int st; waitpid(pids[w],&st,0); pids[w]=-1;
    }

    free(aug_full); aug_full=NULL;

    /* ── PHASE 9: back-substitution ─────────────────────────────────── */
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
