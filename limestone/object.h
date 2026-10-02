#ifndef LIMESTONE_OBJECT_H
#define LIMESTONE_OBJECT_H
#include "limestone.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct limestone_object_target limestone_object_target;
typedef struct limestone_object limestone_object;
typedef struct limestone_object_data limestone_object_data;
typedef struct limestone_linked_image limestone_linked_image;
enum { LIMESTONE_OBJECT_PROGBITS=1, LIMESTONE_OBJECT_NOTE=7, LIMESTONE_OBJECT_NOBITS=8 };
#define LIMESTONE_OBJECT_UNDEFINED 0xffffffffU
#define LIMESTONE_OBJECT_ABSOLUTE 0xfffffffeU
typedef enum limestone_symbol_binding
#ifdef __cplusplus
  : int
#endif
{
  LIMESTONE_SYMBOL_LOCAL=0, LIMESTONE_SYMBOL_GLOBAL=1, LIMESTONE_SYMBOL_WEAK=2
} limestone_symbol_binding;
typedef struct limestone_object_section {
  const char *name;
  uint32_t type;
  uint64_t flags, alignment, entry_size;
  const uint8_t *bytes;
  size_t byte_count;
  uint64_t zero_fill;
} limestone_object_section;
typedef struct limestone_object_symbol {
  const char *name;
  uint32_t section;
  uint64_t value, size;
  limestone_symbol_binding binding;
  uint8_t type, visibility;
} limestone_object_symbol;
typedef struct limestone_object_relocation {
  uint32_t section, symbol, type;
  uint64_t offset;
  int64_t addend;
} limestone_object_relocation;
typedef struct limestone_external_symbol { const char *name; uint64_t address; } limestone_external_symbol;
/* All handles own snapshots, independent of their inputs/parents. Mutable
 * object inspection strings/spans are borrowed until mutation/destruction.
 * Images/data are immutable and borrow views until destruction. Independent
 * handles may be used concurrently; synchronize shared mutable objects.
 * Output storage is preserved on failure. Destroy functions accept NULL. */
limestone_object_target *limestone_object_target_load_isa(const char *source, limestone_error *error);
void limestone_object_target_destroy(limestone_object_target *target);
limestone_object *limestone_object_create(const limestone_object_target *target, limestone_error *error);
limestone_object *limestone_object_load_elf(const uint8_t *bytes, size_t size, const char *source, limestone_error *error);
limestone_object *limestone_object_from_code(const limestone_object_target *target, const uint8_t *bytes, size_t size, const char *symbol, limestone_error *error);
/* Requires an encoded module. Converts its named relocations through the target. */
limestone_object *limestone_module_object(const limestone_module *module, const limestone_object_target *target, const char *symbol, limestone_error *error);
void limestone_object_destroy(limestone_object *object);
const char *limestone_object_text(const limestone_object *object);
size_t limestone_object_section_count(const limestone_object *object);
size_t limestone_object_symbol_count(const limestone_object *object);
size_t limestone_object_relocation_count(const limestone_object *object);
limestone_status limestone_object_get_section(const limestone_object *object, size_t index, limestone_object_section *section, limestone_error *error);
limestone_status limestone_object_get_symbol(const limestone_object *object, size_t index, limestone_object_symbol *symbol, limestone_error *error);
limestone_status limestone_object_get_relocation(const limestone_object *object, size_t index, limestone_object_relocation *relocation, limestone_error *error);
limestone_status limestone_object_add_section(limestone_object *object, const limestone_object_section *section, uint32_t *id, limestone_error *error);
limestone_status limestone_object_add_symbol(limestone_object *object, const limestone_object_symbol *symbol, uint32_t *id, limestone_error *error);
limestone_status limestone_object_add_relocation(limestone_object *object, const limestone_object_relocation *relocation, limestone_error *error);
limestone_object_data *limestone_object_emit_elf(const limestone_object *object, limestone_error *error);
void limestone_object_data_destroy(limestone_object_data *data);
const uint8_t *limestone_object_data_bytes(const limestone_object_data *data);
size_t limestone_object_data_size(const limestone_object_data *data);
/* Lays out allocated sections; applies explicit absolute/PC-relative fields,
 * local/global/weak symbol resolution, scale, range and overlap checks.
 * The returned bytes are not installed or made executable. */
limestone_linked_image *limestone_object_link(const limestone_object_target *target, const limestone_object *const *objects, size_t count,
  uint64_t base_address, uint64_t max_size, const limestone_external_symbol *externals, size_t external_count, limestone_error *error);
void limestone_linked_image_destroy(limestone_linked_image *image);
const uint8_t *limestone_linked_image_bytes(const limestone_linked_image *image);
size_t limestone_linked_image_size(const limestone_linked_image *image);
uint64_t limestone_linked_image_base(const limestone_linked_image *image);
size_t limestone_linked_image_symbol_count(const limestone_linked_image *image);
limestone_status limestone_linked_image_get_symbol(const limestone_linked_image *image, size_t index, limestone_external_symbol *symbol, limestone_error *error);
limestone_status limestone_linked_image_find_symbol(const limestone_linked_image *image, const char *name, uint64_t *address, limestone_error *error);
#ifdef __cplusplus
}
#endif
#endif
