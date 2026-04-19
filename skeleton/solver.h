/*
 * solver.h — Parallel linear-system solver (T26).
 *
 * Solves  A x = b  where A is an n×n dense matrix and b is length-n.
 *
 * ── Algorithm ────────────────────────────────────────────────────────
 *
 * SERIAL PATH  (n < SOLVER_SERIAL_THRESHOLD  or  num_workers == 1)
 *   Calls LAPACKE_dgesv directly — LU decomposition with partial
 *   pivoting, O(n^3/3) flops, numerically stable.
 *
 * PARALLEL PATH  (Gaussian elimination with row-band distribution)
 *
 *   The n rows of the augmented matrix [A|b] are split into
 *   `num_workers` contiguous bands.  Each worker child owns its band
 *   for the entire computation and communicates only the current pivot
 *   row to the others at each elimination step.
 *
 *   Protocol per elimination step k  (k = 0..n-1):
 *
 *     1. The worker that owns row k sends it up to the parent.
 *     2. The parent broadcasts the pivot row down to ALL workers.
 *     3. Each worker eliminates column k from its rows below the pivot:
 *            A[i] -= (A[i][k] / A[k][k]) * A[k]     for i > k in band
 *
 *   After n steps the matrix is upper-triangular.
 *   The parent collects the finished rows and performs back-
 *   substitution serially to obtain x.
 *
 *   Communication: O(n^2) doubles total (one pivot row per step).
 *   Compute per worker: O(n^3 / num_workers).
 *
 * ── Build dependency ─────────────────────────────────────────────────
 *   sudo apt install liblapacke-dev libopenblas-dev
 *   Link with:  -llapacke -lopenblas
 */

#ifndef SOLVER_H
#define SOLVER_H

#define SOLVER_SERIAL_THRESHOLD  32
#define SOLVER_MAX_WORKERS       16

typedef struct {
    int     status; /* 0=ok  -1=singular  -2=alloc  -3=IPC */
    double *x;      /* malloc'd double[n]; caller must free() */
} solver_result_t;

solver_result_t solver_solve(int n, const double *A, const double *b,
                             int num_workers);

#endif /* SOLVER_H */
