#ifndef SATIE_MODULE_C_H
#define SATIE_MODULE_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SatieCModule SatieCModule;

typedef enum SatieCModuleStatus {
    SATIE_C_MODULE_SAT = 1,
    SATIE_C_MODULE_UNSAT = 0,
    SATIE_C_MODULE_UNKNOWN = -1,
    SATIE_C_MODULE_ERROR = -2
} SatieCModuleStatus;

/* Number of theory modules registered in this build. */
size_t satie_module_count (void);

/*
 * Name of the i-th registered theory ("BV", "EUF", ...).
 * Returns NULL when `index` is out of range.
 */
const char *satie_module_name_at (size_t index);

/* Creates a module instance by theory name, or NULL when unknown. */
SatieCModule *satie_module_create (const char *theory_name);
void satie_module_destroy (SatieCModule *module);

/* Theory name of a live module instance, or NULL on NULL input. */
const char *satie_module_name (const SatieCModule *module);

/* Appends one clause to the module problem. Returns 0 on success. */
int satie_module_add_clause (SatieCModule *module, const int *literals, size_t count);

/* Runs the theory check. Returns a SatieCModuleStatus value. */
int satie_module_check (SatieCModule *module);

/* Thread-local diagnostic for the last failing module call. Never NULL. */
const char *satie_module_last_error (void);

#ifdef __cplusplus
}
#endif

#endif /* SATIE_MODULE_C_H */
