# ExtensionTk: Loading, Versions, Registry, and Services {#manual_18_etk}

`etk_version.h` and `etk_registry.h` require no heap allocation. Their state and
borrowed strings belong to the caller. Independent instances can run on
separate threads; shared registries need external synchronization.

## Versions

`etk_version_parse(text, &version)` accepts a complete
`MAJOR.MINOR.PATCH[-prerelease][+build]`. It returns `ETK_EVERSION` for malformed
input, numeric overflow, leading zeros, or whitespace and leaves the output
unchanged. Prerelease text is a borrowed slice with `prerelease_len`; keep the
input alive. Build metadata is validated and discarded.

Comparison includes numeric and textual prerelease identifiers. Formatting
returns an `etk_status`, preserves prerelease text, omits build metadata, and
NUL-terminates a nonempty output buffer on truncation.

Constraints support exact versions, `=`, `<`, `<=`, `>`, `>=`, `^`, `~`, and
bare prefix ranges (`1`, `1.2`, `1.x`, `*`). Whitespace joins comparators with
AND: `>=2.0.0 <3.0.0`. Caret preserves the first nonzero component; tilde
preserves major/minor. Operator operands must be complete versions. A
prerelease requires an explicit prerelease comparator with the same numeric
core. Empty constraints, OR, commas, and hyphen ranges are rejected.

## Registry lifecycle

Define `ETK_VERSION_IMPLEMENTATION` and `ETK_REGISTRY_IMPLEMENTATION` before
including `ExtensionTk/etk_registry.h` in the implementation translation unit.
Use `ETK_DEF=extern` consistently when sharing the functions across source files.

Initialize with `etk_registry_init(&registry)`. Register borrowed descriptors
with `etk_registry_register(&registry, &extension)`. Dependencies are exact
identifier strings in `dependencies`, counted by `dependency_count`; they may
be registered in any order. Duplicate IDs are rejected. No library loading
occurs during registration or activation.

`etk_registry_activate` validates the entire graph before invoking callbacks.
Missing dependencies return `ETK_ENOTFOUND`; cycles return `ETK_EFAIL`.
Dependencies activate first and already-active entries are skipped. If an
activation fails, its status is returned and newly activated entries are
cleaned up in reverse order. Previously active entries remain active. The
failing callback must clean up its own partial resources.

`etk_registry_deactivate_all` follows reverse successful activation order,
retains registrations, and can be called repeatedly. Callbacks must not mutate
the registry. Reentrant register/activate calls fail; reentrant teardown is
ignored. Do not reinitialize an active registry. The registry now contains
`activation_order`, `active_count`, and `busy`, so consumers must rebuild.

## Loading and host services

The existing loader functions take caller-owned `etk_lib_handle` values.
`etk_lib_open(path, flags, &handle)` returns status; symbol lookup returns a
pointer directly. Only `ETK_LIB_FLAG_NOW` and `ETK_LIB_FLAG_LOCAL` are currently
exposed. `etk_lib_path_search` currently copies a name and does not search.

A service descriptor has `name`, `version`, and a `void *value`.
`etk_service_register(&table, &service)` registers it, and
`etk_service_lookup(&table, name, constraint)` returns the first matching value
or NULL. Use a pointer to a caller-owned service struct containing typed
function pointers; direct function-pointer conversion to `void *` is not
portable C99. Names, prerelease text, and service values remain caller-owned.
