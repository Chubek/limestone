#ifndef GRAFITT_MODULE_H
#define GRAFITT_MODULE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Global Grafitt module ABI version.
 */
#define GM_MODULE_ABI_VERSION 1u

/**
 * @brief Current host ABI floor/ceiling this header targets.
 */
#define GM_MODULE_ABI_MIN_SUPPORTED 1u
#define GM_MODULE_ABI_MAX_SUPPORTED 1u

/**
 * @brief Generic status code used across module boundary.
 */
typedef enum GM_Status {
  GM_STATUS_OK = 0,
  GM_STATUS_INVALID_ARGUMENT = 1,
  GM_STATUS_ABI_MISMATCH = 2,
  GM_STATUS_INCOMPATIBLE_MODULE = 3,
  GM_STATUS_CAPACITY_TOO_SMALL = 4,
  GM_STATUS_START_FAILED = 5,
  GM_STATUS_END_FAILED = 6,
  GM_STATUS_UNSUPPORTED_CAPABILITY = 7
} GM_Status;

/**
 * @brief Declared module thread-safety contract.
 */
typedef enum GM_ThreadSafety {
  GM_THREAD_SINGLE = 0,
  GM_THREAD_EXTERNALLY_SYNCHRONIZED = 1,
  GM_THREAD_INTERNAL = 2
} GM_ThreadSafety;

/**
 * @brief Capability identifiers for module registration domains.
 */
typedef enum GM_CapabilityId {
  GM_CAP_ALGO = 1,
  GM_CAP_SERIALIZATION = 2,
  GM_CAP_VISUALIZATION = 3,
  GM_CAP_REWRITE = 4,
  GM_CAP_EQSAT = 5,
  GM_CAP_MATCHING = 6,
  GM_CAP_QUERY = 7,
  GM_CAP_ANALYSIS = 8
} GM_CapabilityId;

typedef struct GM_StringView {
  const char* data;
  uint32_t size;
} GM_StringView;

typedef struct GM_SemVersion {
  uint16_t major;
  uint16_t minor;
  uint16_t patch;
  uint16_t reserved;
} GM_SemVersion;

/**
 * @brief Metadata declaration of one capability exposed by a module.
 */
typedef struct GM_CapabilityDecl {
  uint32_t capability_id;
  uint32_t interface_version;
  uint64_t capability_flags;
  GM_StringView name;
} GM_CapabilityDecl;

/**
 * @brief Optional module dependency declaration.
 */
typedef struct GM_DependencyDecl {
  GM_StringView module_name;
  GM_SemVersion min_version;
  uint32_t required;
  uint32_t reserved0;
} GM_DependencyDecl;

/**
 * @brief Runtime capability table returned by a started module.
 */
typedef struct GM_CapabilityTable {
  uint32_t size;
  uint32_t capability_id;
  uint32_t interface_version;
  uint32_t reserved0;
  const void* table;
  uint64_t flags;
} GM_CapabilityTable;

/**
 * @brief Immutable module metadata returned by `GM_ModMetadata`.
 */
typedef struct GM_ModMetadata_Record {
  uint32_t size;
  uint32_t abi_min_version;
  uint32_t abi_max_version;
  uint32_t module_flags;
  GM_SemVersion module_version;
  GM_ThreadSafety thread_safety;
  uint32_t reserved0;
  GM_StringView module_name;
  GM_StringView vendor;
  GM_StringView build_id;
  GM_StringView license;
  uint32_t capability_count;
  uint32_t reserved1;
  const GM_CapabilityDecl* capabilities;
  uint32_t dependency_count;
  uint32_t reserved2;
  const GM_DependencyDecl* dependencies;
  uint64_t reserved[4];
} GM_ModMetadata_Record;

typedef void (*GM_LogFn)(void* user, int level, GM_StringView message);
typedef void* (*GM_AllocFn)(void* user, size_t size, size_t alignment);
typedef void (*GM_FreeFn)(void* user, void* ptr);
typedef GM_Status (*GM_RegisterCapabilityFn)(void* user, const GM_CapabilityTable* table);
typedef void (*GM_ReportDiagnosticFn)(void* user, int level, GM_StringView code, GM_StringView message);

/**
 * @brief Host API table passed to `GM_ModStart`.
 */
typedef struct GM_HostAPI {
  uint32_t size;
  uint32_t host_abi_version;
  GM_SemVersion host_version;
  void* user_data;
  GM_LogFn log_fn;
  GM_AllocFn alloc_fn;
  GM_FreeFn free_fn;
  GM_RegisterCapabilityFn register_capability_fn;
  GM_ReportDiagnosticFn report_diagnostic_fn;
  uint64_t reserved[6];
} GM_HostAPI;

typedef const GM_ModMetadata_Record* (*GM_ModMetadata_Fn)(void);
typedef GM_Status (*GM_ModStart_Fn)(const GM_HostAPI* host_api, GM_CapabilityTable* out_tables, uint32_t* inout_count);
typedef GM_Status (*GM_ModEnd_Fn)(void);

/**
 * @brief Required export: returns module metadata.
 */
const GM_ModMetadata_Record* GM_ModMetadata(void);

/**
 * @brief Required export: starts module and exposes capability tables.
 *
 * If `out_tables == NULL`, `inout_count` is filled with required count.
 */
GM_Status GM_ModStart(const GM_HostAPI* host_api, GM_CapabilityTable* out_tables, uint32_t* inout_count);

/**
 * @brief Required export: shuts module down.
 */
GM_Status GM_ModEnd(void);

static inline GM_StringView GM_sv_from_cstr(const char* s) {
  GM_StringView out;
  uint32_t n = 0;
  if (s != NULL) {
    while (s[n] != '\0') ++n;
  }
  out.data = s;
  out.size = n;
  return out;
}

static inline GM_Status GM_validate_metadata(const GM_ModMetadata_Record* m, uint32_t host_abi_version) {
  if (m == NULL) return GM_STATUS_INVALID_ARGUMENT;
  if (m->size < (uint32_t)sizeof(GM_ModMetadata_Record)) return GM_STATUS_INVALID_ARGUMENT;
  if (m->module_name.data == NULL || m->module_name.size == 0) return GM_STATUS_INVALID_ARGUMENT;
  if (host_abi_version < m->abi_min_version || host_abi_version > m->abi_max_version) {
    return GM_STATUS_ABI_MISMATCH;
  }
  if (m->capability_count > 0 && m->capabilities == NULL) return GM_STATUS_INVALID_ARGUMENT;
  if (m->dependency_count > 0 && m->dependencies == NULL) return GM_STATUS_INVALID_ARGUMENT;
  return GM_STATUS_OK;
}

static inline GM_Status GM_validate_host_api(const GM_HostAPI* host_api) {
  if (host_api == NULL) return GM_STATUS_INVALID_ARGUMENT;
  if (host_api->size < (uint32_t)sizeof(GM_HostAPI)) return GM_STATUS_INVALID_ARGUMENT;
  if (host_api->host_abi_version < GM_MODULE_ABI_MIN_SUPPORTED ||
      host_api->host_abi_version > GM_MODULE_ABI_MAX_SUPPORTED) {
    return GM_STATUS_ABI_MISMATCH;
  }
  if (host_api->register_capability_fn == NULL) return GM_STATUS_INVALID_ARGUMENT;
  return GM_STATUS_OK;
}

static inline GM_Status GM_validate_capability_tables(const GM_CapabilityTable* tables, uint32_t count) {
  uint32_t i;
  if (count == 0) return GM_STATUS_OK;
  if (tables == NULL) return GM_STATUS_INVALID_ARGUMENT;
  for (i = 0; i < count; ++i) {
    if (tables[i].size < (uint32_t)sizeof(GM_CapabilityTable)) return GM_STATUS_INVALID_ARGUMENT;
    if (tables[i].capability_id == 0u) return GM_STATUS_INVALID_ARGUMENT;
    if (tables[i].table == NULL) return GM_STATUS_INVALID_ARGUMENT;
  }
  return GM_STATUS_OK;
}

#ifdef __cplusplus
}
#endif

#endif
