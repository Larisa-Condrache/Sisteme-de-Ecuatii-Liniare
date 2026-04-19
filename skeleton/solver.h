/*
 * solver.h — Parallel linear-system solver (T26).
 *
 * Solves  A x = b  where A is an n×n dense matrix and b is length-n.
 *
 * Strategy
 * --------
 * The work is split across `num_workers` child processes (fork).
 * Each child receives its block of rows via a pipe, calls
 * LAPACKE_dgesv on the full system (LU factorisation + back-
 * substitution), and writes the solution back through another pipe.
 * The parent collects all partial results and returns the solution.
 *
 * For small n (< SOLVER_SERIAL_THRESHOLD) the system is solved
 * directly in the calling process without forking.
 *
 * Build dependency:
 *   sudo apt install liblapacke-dev libopenblas-dev
 *   Link with: -llapacke -lopenblas
 */

#ifndef SOLVER_H
#define SOLVER_H

#include <stddef.h>

/* Solve serially when n is smaller than this threshold. */
#define SOLVER_SERIAL_THRESHOLD 32

/* Maximum number of worker processes. */
#define SOLVER_MAX_WORKERS      16

/*
 * solver_result_t — returned by solver_solve().
 *
 * On success  : status == 0,  x points to a malloc'd double[n].
 * On failure  : status != 0,  x == NULL.
 *   status == -1   → singular matrix (LAPACKE info > 0)
 *   status == -2   → memory allocation error
 *   status == -3   → fork / pipe / IPC error
 */
typedef struct {
    int     status;
    double *x;          /* caller must free() when status == 0 */
} solver_result_t;

/*
 * solver_solve()
 *
 *   n           — system order
 *   A           — n×n matrix, row-major (NOT modified)
 *   b           — right-hand side vector, length n (NOT modified)
 *   num_workers — number of child processes to fork (1 = serial path)
 *
 * Returns a solver_result_t.  Free result.x after use.
 */
solver_result_t solver_solve(int n, const double *A, const double *b,
                             int num_workers);

#endif /* SOLVER_H */
