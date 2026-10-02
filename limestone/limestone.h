#ifndef LIMESTONE_H
#define LIMESTONE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct limestone_module limestone_module;
typedef struct limestone_options { int optimize, schedule, allocate; } limestone_options;
typedef enum limestone_status {
  LIMESTONE_OK=0, LIMESTONE_INVALID_ARGUMENT=1, LIMESTONE_PARSE=2,
  LIMESTONE_NOT_FOUND=3, LIMESTONE_CONFLICT=4, LIMESTONE_UNSUPPORTED=5,
  LIMESTONE_UNSATISFIABLE=6, LIMESTONE_INTERNAL=7, LIMESTONE_TIMEOUT=8,
  LIMESTONE_INTERRUPTED=9, LIMESTONE_RESOURCE_LIMIT=10
} limestone_status;
typedef struct limestone_error { limestone_status code; char message[512]; } limestone_error;
/** Initialize default options. Physical allocation requires a target adapter. */
void limestone_options_default(limestone_options*);
/** Compile a closed TraceML integer program. Caller owns the returned module. */
limestone_module *limestone_compile(const char *input);
/** Checked compile. Options may be NULL. Errors are copied into caller storage.
 * C++ exceptions never cross the ABI. Independent invocations are reentrant.
 */
limestone_module *limestone_compile_checked(const char *input, const limestone_options*, limestone_error*);
/** Borrow text until module destruction; NULL for a NULL module. */
const char *limestone_module_text(const limestone_module *);
/** Destroy a module. NULL is allowed. */
void limestone_module_destroy(limestone_module *);
#ifdef __cplusplus
}
#endif
#endif
