#ifndef ETK_REGISTRY_H
#define ETK_REGISTRY_H
/**
 * @file etk_registry.h
 * @brief Extension registry: registers extension descriptors (id, version,
 * dependencies, entry points), resolves the dependency graph, and
 * activates extensions in topological order through their lifecycle
 * callbacks. All state lives in a caller-supplied registry instance.
 * @stability experimental
 * @depends ExtensionTk::platform, ExtensionTk::types, ExtensionTk::version
 */
#include "etk_platform.h"
#include "etk_types.h"
#include "etk_version.h"

/** @brief Maximum number of registered extensions; must be positive. */
#ifndef ETK_REGISTRY_MAX_EXTENSIONS
#define ETK_REGISTRY_MAX_EXTENSIONS 64
#endif

/**
 * @brief Borrowed extension descriptor with registry-managed activation state.
 * @var etk_extension::id Nonempty, unique, NUL-terminated identifier.
 * @var etk_extension::version Parsed version; prerelease text is borrowed.
 * @var etk_extension::dependencies Array of required extension identifiers.
 * @var etk_extension::dependency_count Number of dependency identifiers.
 * @var etk_extension::activate Optional initialization callback returning status.
 * @var etk_extension::deactivate Optional cleanup for successful initialization.
 * @var etk_extension::ctx Opaque caller-owned callback context.
 * @var etk_extension::active Managed by the registry; ignored on registration.
 * @note Strings, dependency arrays, and contexts must outlive registration.
 * Callbacks must not mutate this registry. A failed activate callback must
 * release its own partially initialized resources.
 */
typedef struct etk_extension {
    const char *id;
    etk_version version;
    const char *const *dependencies;
    size_t dependency_count;
    etk_status (*activate)(void *);
    void (*deactivate)(void *);
    void *ctx;
    etk_bool active;
} etk_extension;

/**
 * @brief Fixed-capacity registry and activation journal owned by the caller.
 * @var etk_registry::entries Registered descriptors, in registration order.
 * @var etk_registry::count Number of registered descriptors.
 * @var etk_registry::activation_order Entry indices in successful activation order.
 * @var etk_registry::active_count Number of journal entries.
 * @var etk_registry::busy Lifecycle callback execution guard.
 * @note All fields are managed by this API. Synchronize access to a shared
 * instance. Independent registries are safe to use concurrently.
 */
typedef struct etk_registry {
    etk_extension entries[ETK_REGISTRY_MAX_EXTENSIONS];
    size_t count;
    size_t activation_order[ETK_REGISTRY_MAX_EXTENSIONS];
    size_t active_count;
    etk_bool busy;
} etk_registry;

/**
 * @brief Initialize an empty registry without allocation.
 * @param r Uninitialized or fully deactivated registry; NULL is a no-op.
 * @note Do not reinitialize from callbacks or while resources are active.
 */
ETK_DEF void etk_registry_init(etk_registry *r);
/**
 * @brief Shallow-copy an extension descriptor into the registry.
 * @param r Initialized registry.
 * @param e Descriptor; dependencies may be registered later.
 * @return ETK_OK, ETK_EINVAL for invalid data, ETK_ENOMEM at capacity, or
 * ETK_EFAIL for a duplicate identifier or reentrant mutation.
 * @note The stored active flag starts false. No ownership transfers.
 */
ETK_DEF etk_status etk_registry_register(etk_registry *r, const etk_extension *e);
/**
 * @brief Validate the whole graph, then activate in dependency order.
 * @param r Initialized registry.
 * @return ETK_OK, ETK_EINVAL for NULL, ETK_ENOTFOUND for a missing dependency,
 * ETK_EFAIL for cycles/reentrancy, or the failing callback's status.
 * @note No callbacks run for an invalid graph. Already-active entries are
 * skipped. On callback failure, only entries newly activated by this call
 * are deactivated in reverse order; previously active entries stay active.
 */
ETK_DEF etk_status etk_registry_activate(etk_registry *r);
/**
 * @brief Find a registered extension by exact identifier.
 * @param r Initialized registry.
 * @param id NUL-terminated identifier.
 * @return Borrowed descriptor pointer, or NULL if absent/arguments are NULL.
 * @note Treat the returned descriptor as read-only. Valid until reinitialization.
 */
ETK_DEF etk_extension *etk_registry_find(etk_registry *r, const char *id);
/**
 * @brief Count registered extensions.
 * @param r Initialized registry, or NULL.
 * @return Registration count; zero for NULL.
 */
ETK_DEF size_t etk_registry_count(const etk_registry *r);
/**
 * @brief Deactivate in reverse successful activation order.
 * @param r Initialized registry; NULL is a no-op.
 * @note Registration is retained for later reactivation. Calls during a
 * lifecycle callback are ignored. Repeated calls are harmless.
 */
ETK_DEF void etk_registry_deactivate_all(etk_registry *r);

#ifdef ETK_REGISTRY_IMPLEMENTATION
#include <string.h>

ETK_DEF void etk_registry_init(etk_registry *r)
{
    if (r != NULL) memset(r, 0, sizeof(*r));
}
ETK_DEF etk_extension *etk_registry_find(etk_registry *r, const char *id)
{
    size_t i;
    if (r == NULL || id == NULL) return NULL;
    for (i = 0; i < r->count; ++i)
        if (strcmp(r->entries[i].id, id) == 0) return &r->entries[i];
    return NULL;
}
ETK_DEF etk_status etk_registry_register(etk_registry *r, const etk_extension *e)
{
    size_t i;
    if (r == NULL || e == NULL || e->id == NULL || !*e->id ||
        (e->dependency_count && e->dependencies == NULL)) return ETK_EINVAL;
    if (r->busy) return ETK_EFAIL;
    for (i = 0; i < e->dependency_count; ++i)
        if (e->dependencies[i] == NULL || !*e->dependencies[i]) return ETK_EINVAL;
    if (etk_registry_find(r, e->id) != NULL) return ETK_EFAIL;
    if (r->count >= ETK_REGISTRY_MAX_EXTENSIONS) return ETK_ENOMEM;
    r->entries[r->count] = *e;
    r->entries[r->count++].active = ETK_FALSE;
    return ETK_OK;
}
ETK_DEF size_t etk_registry_count(const etk_registry *r)
{
    return r != NULL ? r->count : 0;
}
static void _etk_registry_rollback(etk_registry *r, size_t keep)
{
    while (r->active_count > keep) {
        etk_extension *e = &r->entries[r->activation_order[--r->active_count]];
        if (e->deactivate != NULL) e->deactivate(e->ctx);
        e->active = ETK_FALSE;
    }
}
ETK_DEF etk_status etk_registry_activate(etk_registry *r)
{
    size_t order[ETK_REGISTRY_MAX_EXTENSIONS];
    unsigned char scheduled[ETK_REGISTRY_MAX_EXTENSIONS] = {0};
    size_t n = 0, i, j, keep;
    if (r == NULL) return ETK_EINVAL;
    if (r->busy) return ETK_EFAIL;
    /* Preflight before any callback, including unrelated extensions. */
    for (i = 0; i < r->count; ++i)
        for (j = 0; j < r->entries[i].dependency_count; ++j)
            if (etk_registry_find(r, r->entries[i].dependencies[j]) == NULL)
                return ETK_ENOTFOUND;
    while (n < r->count) {
        size_t before = n;
        for (i = 0; i < r->count; ++i) {
            const etk_extension *e = &r->entries[i];
            if (scheduled[i]) continue;
            for (j = 0; j < e->dependency_count; ++j) {
                const etk_extension *dep = etk_registry_find(r, e->dependencies[j]);
                if (!scheduled[(size_t)(dep - r->entries)]) break;
            }
            if (j == e->dependency_count) {
                scheduled[i] = 1;
                order[n++] = i;
            }
        }
        if (n == before) return ETK_EFAIL;
    }
    keep = r->active_count;
    r->busy = ETK_TRUE;
    for (i = 0; i < n; ++i) {
        etk_extension *e = &r->entries[order[i]];
        etk_status status;
        if (e->active) continue;
        status = e->activate != NULL ? e->activate(e->ctx) : ETK_OK;
        if (status != ETK_OK) {
            _etk_registry_rollback(r, keep);
            r->busy = ETK_FALSE;
            return status;
        }
        e->active = ETK_TRUE;
        r->activation_order[r->active_count++] = order[i];
    }
    r->busy = ETK_FALSE;
    return ETK_OK;
}
ETK_DEF void etk_registry_deactivate_all(etk_registry *r)
{
    if (r == NULL || r->busy) return;
    r->busy = ETK_TRUE;
    _etk_registry_rollback(r, 0);
    r->busy = ETK_FALSE;
}
#endif /* ETK_REGISTRY_IMPLEMENTATION */
#endif /* ETK_REGISTRY_H */
