/*
Rezolvam  A x = b  unde A este o matrice densa n×n, iar b are lungimea n.
CALE SERIALA
  Apeleaza direct LAPACKE_dgesv — descompunere LU cu pivotare
  partiala, O(n^3/3) operatii, stabil numeric.
CALE PARALELA 
  Cele n randuri ale matricei augmentate [A|b] sunt impartite in
  `num_workers` benzi contigue. Fiecare worker (proces copil) detine
  banda lui pe toata durata calculului si comunica doar randul pivot
  curent catre ceilalti la fiecare pas de eliminare
 */

#ifndef SOLVER_H
#define SOLVER_H

#define SOLVER_SERIAL_THRESHOLD  32
#define SOLVER_MAX_WORKERS       16

typedef struct {
    int     status; /* 0=ok  -1=singular  -2=alocare  -3=IPC */
    double *x;      
} solver_result_t;

solver_result_t solver_solve(int n, const double *A, const double *b,
                             int num_workers);

#endif /* SOLVER_H */
