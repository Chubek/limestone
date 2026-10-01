#ifndef SATIE_PLUGIN_C_H
#define SATIE_PLUGIN_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SatieCPluginContext SatieCPluginContext;

/*
 * Command callback. `args` points to `count` NUL-terminated strings.
 * Returns a NUL-terminated result string (may be a static or
 * callback-owned buffer; the context copies it before returning).
 * Returns NULL on failure; the context then reports a generic error.
 */
typedef const char *(*SatieCCommandFn) (const char *const *args, size_t count,
                                        void *userdata);

SatieCPluginContext *satie_plugin_context_create (void);
void satie_plugin_context_destroy (SatieCPluginContext *context);

/* Registers `name`. Returns 0 on success, nonzero when duplicate/invalid. */
int satie_plugin_register (SatieCPluginContext *context, const char *name,
                           SatieCCommandFn function, void *userdata);

/* Returns 1 when `name` is registered, 0 otherwise. */
int satie_plugin_has (const SatieCPluginContext *context, const char *name);

/*
 * Invokes `name` and writes the NUL-terminated result into `out`.
 * Returns 0 on success, nonzero on unknown command, callback failure,
 * or truncation (`out_size` too small).
 */
int satie_plugin_invoke (const SatieCPluginContext *context, const char *name,
                         const char *const *args, size_t count,
                         char *out, size_t out_size);

/* Thread-local diagnostic for the last failing plugin call. Never NULL. */
const char *satie_plugin_last_error (void);

#ifdef __cplusplus
}
#endif

#endif /* SATIE_PLUGIN_C_H */
