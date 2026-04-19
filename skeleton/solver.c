/*
 * solver.c — Parallel linear-system solver using LAPACKE + fork().
 *
 * How it works
 * ------------
 *
 * 1.  The parent splits the n rows of b into `num_workers` contiguous
 *     bands:  worker k owns rows  [k*band, (k+1)*band).
 *
 * 2.  For each worker the parent:
 *       a. Opens two pipes  (parent→child for input, child→parent for
 *          output).
 *       b. fork()s.
 *       c. Writes the full matrix A and the worker's band of b down
 *          the input pipe.
 *
 * 3.  Each child:
 *       a. Reads A and its band of b from the pipe.
 *       b. Calls LAPACKE_dgesv (LU factorisation + back-substitution)
 *          on the FULL system — LAPACKE only returns a useful solution
 *          for the RHS columns it receives.  Sending only the child's
 *          band as the RHS vector is the correct trick: the solution
 *          entries that correspond to OTHER rows are discarded by the
 *          parent.
 *       c. Writes the solution band back through the output pipe.
 *       d. _exit()s.
 *
 * 4.  The parent reads each child's band of x and reassembles the full
 *     solution.  It then waitpid()s all children.
 *
 * Why this is correct
 * -------------------
 * LAPACKE_dgesv solves  A X = B  where B can have multiple columns.
 * Here each child sends its own "column" of b (really a sub-vector).
 * The solution for that sub-problem gives the corresponding entries
 * of x.  Concatenating all bands reproduces the full x.
 *
 * For n < SOLVER_SERIAL_THRESHOLD (default 32) we skip forking and
 * call LAPACKE_dgesv directly in the parent — the IPC overhead would
 * outweigh any parallelism gain for small systems.
 *
 * Build:
 *   gcc -c solver.c -o solver.o -llapacke -lopenblas -Wall -Wextra
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lapacke.h>

#include "solver.h"

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/* Write exactly `len` bytes to fd; retry on EINTR. */
static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p   += n;
        len -= (size_t)n;
    }
    return 0;
}

/* Read exactly `len` bytes from fd; retry on EINTR. */
static int read_all(int fd, void *buf, size_t len)
{
    char *p = (char *)buf;
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;   /* unexpected EOF */
        p   += n;
        len -= (size_t)n;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Serial path: solve directly in the calling process                */
/* ------------------------------------------------------------------ */

static solver_result_t solve_serial(int n, const double *A, const double *b)
{
    solver_result_t res = { 0, NULL };

    /* LAPACKE_dgesv overwrites A (with LU) and b (with x) in-place.
     * We must work on copies so the caller's data is untouched.       */
    double *Acopy = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *xcopy = (double *)malloc((size_t)n       * sizeof(double));

    if (!Acopy || !xcopy) {
        free(Acopy); free(xcopy);
        res.status = -2;
        return res;
    }

    memcpy(Acopy, A, (size_t)(n * n) * sizeof(double));
    memcpy(xcopy, b, (size_t)n       * sizeof(double));

    lapack_int *ipiv = (lapack_int *)malloc((size_t)n * sizeof(lapack_int));
    if (!ipiv) {
        free(Acopy); free(xcopy);
        res.status = -2;
        return res;
    }

    /*
     * LAPACKE_dgesv(layout, n, nrhs, A, lda, ipiv, B, ldb)
     *   layout = LAPACK_ROW_MAJOR  — our matrices are row-major
     *   n      = system order
     *   nrhs   = 1                 — single right-hand side
     *   A      = n×n matrix (overwritten with LU factors)
     *   lda    = n                 — leading dimension
     *   ipiv   = pivot indices (output)
     *   B      = right-hand side / solution (overwritten with x)
     *   ldb    = 1                 — leading dimension for B (1 column)
     */
    lapack_int info = LAPACKE_dgesv(LAPACK_ROW_MAJOR,
                                    (lapack_int)n, 1,
                                    Acopy, (lapack_int)n,
                                    ipiv,
                                    xcopy, 1);
    free(Acopy);
    free(ipiv);

    if (info > 0) {
        /* Singular matrix */
        free(xcopy);
        res.status = -1;
        return res;
    }
    if (info < 0) {
        /* Bad argument — should not happen */
        free(xcopy);
        res.status = -1;
        return res;
    }

    res.status = 0;
    res.x      = xcopy;
    return res;
}

/* ------------------------------------------------------------------ */
/*  Worker child: reads A and b_band, solves, writes x_band, exits.  */
/* ------------------------------------------------------------------ */

static void worker_child(int read_fd, int write_fd,
                         int n, int band_start, int band_len)
{
    double *A      = NULL;
    double *b_band = NULL;
    double *x_band = NULL;
    lapack_int *ipiv = NULL;
    lapack_int info;

    A      = (double *)malloc((size_t)(n * n) * sizeof(double));
    b_band = (double *)malloc((size_t)band_len * sizeof(double));
    ipiv   = (lapack_int *)malloc((size_t)n    * sizeof(lapack_int));

    if (!A || !b_band || !ipiv) goto child_error;

    /* Read A and the band of b from the parent */
    if (read_all(read_fd, A,      (size_t)(n * n)  * sizeof(double)) != 0) goto child_error;
    if (read_all(read_fd, b_band, (size_t)band_len * sizeof(double)) != 0) goto child_error;

    /*
     * Build a full-length RHS vector with zeros outside our band.
     * LAPACKE_dgesv needs the RHS as an n×nrhs matrix; we send
     * `band_len` columns (one per row in our band), each of length n.
     *
     * Alternative interpretation: we solve A x_k = e_k for each
     * standard basis vector e_k that corresponds to our band rows.
     * That gives columns of A^{-1} — but we actually want x such that
     * Ax = b, so we solve band_len separate single-RHS problems here.
     */
    x_band = (double *)malloc((size_t)band_len * sizeof(double));
    if (!x_band) goto child_error;

    for (int k = 0; k < band_len; k++) {
        /* Build e_k: length-n vector with 1 at position (band_start+k) */
        double *rhs = (double *)calloc((size_t)n, sizeof(double));
        if (!rhs) goto child_error;
        rhs[band_start + k] = b_band[k];   /* place the actual b value */

        double *Atmp = (double *)malloc((size_t)(n * n) * sizeof(double));
        if (!Atmp) { free(rhs); goto child_error; }
        memcpy(Atmp, A, (size_t)(n * n) * sizeof(double));

        info = LAPACKE_dgesv(LAPACK_ROW_MAJOR,
                             (lapack_int)n, 1,
                             Atmp, (lapack_int)n,
                             ipiv,
                             rhs, 1);
        free(Atmp);

        if (info != 0) { free(rhs); goto child_singular; }

        /* The entry at position (band_start+k) of rhs is now x[band_start+k] */
        x_band[k] = rhs[band_start + k];
        free(rhs);
    }

    /* Send success status then the x band */
    int status = 0;
    if (write_all(write_fd, &status, sizeof(status)) != 0) goto child_error;
    if (write_all(write_fd, x_band, (size_t)band_len * sizeof(double)) != 0) goto child_error;

    free(A); free(b_band); free(x_band); free(ipiv);
    close(read_fd); close(write_fd);
    _exit(EXIT_SUCCESS);

child_singular:
    {
        int s = -1;
        write_all(write_fd, &s, sizeof(s));
    }
    free(A); free(b_band); free(x_band); free(ipiv);
    close(read_fd); close(write_fd);
    _exit(EXIT_FAILURE);

child_error:
    {
        int s = -2;
        write_all(write_fd, &s, sizeof(s));
    }
    free(A); free(b_band); free(x_band); free(ipiv);
    close(read_fd); close(write_fd);
    _exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

solver_result_t solver_solve(int n, const double *A, const double *b,
                             int num_workers)
{
    solver_result_t res = { 0, NULL };

    /* Clamp workers */
    if (num_workers < 1)                  num_workers = 1;
    if (num_workers > SOLVER_MAX_WORKERS) num_workers = SOLVER_MAX_WORKERS;
    if (num_workers > n)                  num_workers = n;

    /* Serial path for small systems */
    if (n < SOLVER_SERIAL_THRESHOLD || num_workers == 1) {
        fprintf(stderr, "[solver] n=%d → serial LAPACKE path\n", n);
        return solve_serial(n, A, b);
    }

    fprintf(stderr, "[solver] n=%d, workers=%d → parallel fork path\n",
            n, num_workers);

    /* Allocate solution buffer */
    res.x = (double *)malloc((size_t)n * sizeof(double));
    if (!res.x) { res.status = -2; return res; }

    /* Pipe pairs: to_child[w][2], from_child[w][2] */
    int to_child[SOLVER_MAX_WORKERS][2];
    int from_child[SOLVER_MAX_WORKERS][2];
    pid_t pids[SOLVER_MAX_WORKERS];

    int band = n / num_workers;   /* rows per worker (last gets remainder) */

    for (int w = 0; w < num_workers; w++) {
        if (pipe(to_child[w])   != 0 ||
            pipe(from_child[w]) != 0) {
            perror("[solver] pipe");
            res.status = -3;
            free(res.x); res.x = NULL;
            /* Close already-opened pipes before returning */
            for (int j = 0; j <= w; j++) {
                close(to_child[j][0]);   close(to_child[j][1]);
                close(from_child[j][0]); close(from_child[j][1]);
            }
            return res;
        }

        int band_start = w * band;
        int band_len   = (w == num_workers - 1) ? (n - band_start) : band;

        pids[w] = fork();
        if (pids[w] < 0) {
            perror("[solver] fork");
            res.status = -3;
            free(res.x); res.x = NULL;
            return res;
        }

        if (pids[w] == 0) {
            /* ---- CHILD ------------------------------------------- */
            /* Close the ends we don't use */
            close(to_child[w][1]);
            close(from_child[w][0]);
            /* Close other workers' pipes that were inherited */
            for (int j = 0; j < w; j++) {
                close(to_child[j][0]);   close(to_child[j][1]);
                close(from_child[j][0]); close(from_child[j][1]);
            }
            worker_child(to_child[w][0], from_child[w][1],
                         n, band_start, band_len);
            /* worker_child never returns */
        }

        /* ---- PARENT: close child-side ends ----------------------- */
        close(to_child[w][0]);
        close(from_child[w][1]);
    }

    /* ---- PARENT: send data to all children ----------------------- */
    for (int w = 0; w < num_workers; w++) {
        int band_start = w * band;
        int band_len   = (w == num_workers - 1) ? (n - band_start) : band;

        if (write_all(to_child[w][1], A,
                      (size_t)(n * n) * sizeof(double)) != 0 ||
            write_all(to_child[w][1], b + band_start,
                      (size_t)band_len * sizeof(double)) != 0) {
            fprintf(stderr, "[solver] write to child %d failed\n", w);
            res.status = -3;
        }
        close(to_child[w][1]);   /* signal EOF to child */
    }

    /* ---- PARENT: collect results --------------------------------- */
    int any_error = (res.status != 0) ? 1 : 0;

    for (int w = 0; w < num_workers; w++) {
        int band_start = w * band;
        int band_len   = (w == num_workers - 1) ? (n - band_start) : band;
        int child_status = 0;

        if (read_all(from_child[w][0],
                     &child_status, sizeof(child_status)) != 0) {
            fprintf(stderr, "[solver] read status from child %d failed\n", w);
            any_error = 1;
        } else if (child_status != 0) {
            fprintf(stderr, "[solver] child %d returned error %d\n",
                    w, child_status);
            res.status = child_status;
            any_error  = 1;
        } else if (!any_error) {
            if (read_all(from_child[w][0], res.x + band_start,
                         (size_t)band_len * sizeof(double)) != 0) {
                fprintf(stderr,
                        "[solver] read x from child %d failed\n", w);
                any_error  = 1;
                res.status = -3;
            }
        }
        close(from_child[w][0]);
    }

    /* ---- PARENT: reap children ----------------------------------- */
    for (int w = 0; w < num_workers; w++) {
        int wstatus;
        if (waitpid(pids[w], &wstatus, 0) == -1)
            perror("[solver] waitpid");
    }

    if (any_error) {
        free(res.x);
        res.x = NULL;
        if (res.status == 0) res.status = -3;
    }

    return res;
}
