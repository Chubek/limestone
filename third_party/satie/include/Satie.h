#ifndef SATIE_C_H
#define SATIE_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SatieCStatus {
    SATIE_C_SAT = 1,
    SATIE_C_UNSAT = 0,
    SATIE_C_UNKNOWN = -1
} SatieCStatus;

typedef enum SatieCEngine {
    SATIE_C_NATIVE = 0,
    SATIE_C_DPLL = 1,
    SATIE_C_CDCL = 2
} SatieCEngine;

typedef struct SatieSolver SatieSolver;

/* Library metadata. Returned pointers remain valid for the process lifetime. */
const char *satie_version (void);
const char *satie_build_info (void);

/* Thread-local diagnostic for the last failing C API call. Never NULL. */
const char *satie_last_error (void);

SatieSolver *satie_solver_create (void);
void satie_solver_destroy (SatieSolver *solver);

/* Discards loaded clauses and any cached result. */
void satie_solver_reset (SatieSolver *solver);

/* Load a formula. Returns 0 on success, nonzero on parse/alloc failure. */
int satie_solver_load_dimacs (SatieSolver *solver, const char *text);
int satie_solver_load_cnf (SatieSolver *solver, const char *text);

/* Appends one clause to the loaded problem. Returns 0 on success. */
int satie_solver_add_clause (SatieSolver *solver, const int *literals, size_t count);

/* Solves with the requested engine. Returns a SatieCStatus value. */
int satie_solver_solve (SatieSolver *solver, int engine);

/* Status of the most recent solve, or SATIE_C_UNKNOWN when never solved. */
int satie_solver_status (const SatieSolver *solver);

/* Number of variables in the loaded problem (0 on NULL solver). */
size_t satie_solver_variable_count (const SatieSolver *solver);

/*
 * Value of `var` in the last satisfying assignment:
 *   1 = true, 0 = false, -1 = unknown / no model.
 */
int satie_solver_variable_value (const SatieSolver *solver, int var);

#ifdef __cplusplus
}
#endif

#endif /* SATIE_C_H */
