#ifndef EXL_EXOLAYER_H
#define EXL_EXOLAYER_H
#include <stddef.h>
#include <stdint.h>
#ifndef EXL_DEF
#define EXL_DEF
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct exl_context exl_context_t;
typedef struct exl_library exl_library_t;
typedef enum exl_kind { EXL_VOID, EXL_I64, EXL_F64, EXL_PTR } exl_kind_t;
typedef struct exl_value { exl_kind_t kind; union { long long i64; double f64; void *ptr; } as; } exl_value_t;
/** @brief Synchronous host callback. Arguments and result are borrowed for the call.
 * Userdata remains owned by the registrant. Pointer values are borrowed opaque
 * host pointers; no target ABI conversion or arbitrary native invocation occurs.
 */
typedef void (*exl_native_fn)(const exl_value_t *args, size_t nargs, exl_value_t *result, void *userdata);
/** @brief Extension descriptor ABI version. */
#define EXL_EXTENSION_ABI_VERSION 1u
/** @brief Export this immutable descriptor as `exl_extension_descriptor`.
 * Descriptor strings, symbols, and userdata are borrowed until library close.
 * Callbacks obey exl_native_fn's host-only ABI. No initialization is invoked.
 */
typedef struct exl_extension_symbol {
  const char *name;
  exl_native_fn function;
  void *userdata;
} exl_extension_symbol_t;
typedef struct exl_extension_descriptor {
  uint32_t abi_version;
  size_t symbol_count;
  const exl_extension_symbol_t *symbols;
} exl_extension_descriptor_t;
/** @brief Create a context; NULL on allocation failure. */
EXL_DEF exl_context_t *exl_context_create(void);
/** @brief Destroy the context and its libraries. NULL is allowed.
 * @note Synchronize shared-context access. Do not destroy from a callback.
 */
EXL_DEF void exl_context_destroy(exl_context_t*);
/** @brief Register a callback; returns 0, -1 invalid, -2 conflict, -3 failure.
 * @note Names are copied. The caller keeps userdata alive until context teardown.
 */
EXL_DEF int exl_register(exl_context_t*, const char*, exl_native_fn, void*);
/** @brief Invoke a callback; returns 0, -1 invalid, -2 missing, -3 failure.
 * @note On failure result is EXL_VOID. Calls may reenter the context. C++
 * exceptions from callbacks are contained. VOID is not a valid argument kind.
 */
EXL_DEF int exl_call(exl_context_t*, const char*, const exl_value_t*, size_t, exl_value_t*);
/** @brief Borrow the last diagnostic, valid until the next context operation. */
EXL_DEF const char *exl_last_error(const exl_context_t*);
/** @brief Load through ExtensionTk, validate ABI and all exports, then register.
 * @return Context-owned library or NULL with a diagnostic; no partial registration.
 * @note A library remains loaded until close or context destruction.
 */
EXL_DEF exl_library_t *exl_library_open(exl_context_t*, const char *path);
/** @brief Unregister a library's callbacks and close it; returns 0 or -1/-3.
 * @note The library handle becomes invalid. Closing an active callback's library
 * is rejected. NULL/foreign library handles are rejected without dereferencing.
 */
EXL_DEF int exl_library_close(exl_context_t*, exl_library_t*);
#ifdef __cplusplus
}
#endif
#ifdef EXL_EXOLAYER_IMPLEMENTATION
#ifndef __cplusplus
#error "Exolayer's implementation translation unit requires C++20"
#endif
#include "exolayer_impl.hpp"
#endif
#endif
