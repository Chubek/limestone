/**
 * @file etk_dynload.h
 * @brief Portable extension-library loading and symbol lookup.
 * @stability experimental
 * @depends ExtensionTk::platform, ExtensionTk::types, InteropTk::error
 */
#ifndef ETK_DYNLOAD_H
#define ETK_DYNLOAD_H
#include "etk_types.h"
#include "../FFItk/ffi_loader.h"
#define ETK_PATH_MAX 1024
#define ETK_SYMNAME_MAX 256
#define ETK_LIB_FLAG_NOW FFI_LIB_NOW
#define ETK_LIB_FLAG_LOCAL FFI_LIB_LOCAL
#define ETK_HAS_DLOPEN 1
#define ETK_HAS_LOADLIBRARY 0
typedef ffi_library etk_lib_handle;
typedef void *etk_sym_handle;
ETK_DEF etk_status etk_lib_open(const char *path, unsigned flags,
                                etk_lib_handle *out);
ETK_DEF etk_status etk_lib_close(etk_lib_handle *lib);
ETK_DEF etk_sym_handle etk_lib_sym(const etk_lib_handle *lib, const char *name);
ETK_DEF etk_sym_handle etk_lib_sym_optional(const etk_lib_handle *lib,
                                            const char *name);
ETK_DEF etk_bool etk_lib_path_search(const char *name, char *out, size_t cap);
#ifdef ETK_DYNLOAD_IMPLEMENTATION
#include <string.h>
ETK_DEF etk_status etk_lib_open(const char *path, unsigned flags,
                                etk_lib_handle *out)
{ return ffi_open(path, flags, out) == FFI_OK ? ETK_OK : ETK_EFAIL; }
ETK_DEF etk_status etk_lib_close(etk_lib_handle *lib)
{ return ffi_close(lib) == FFI_OK ? ETK_OK : ETK_EFAIL; }
ETK_DEF etk_sym_handle etk_lib_sym(const etk_lib_handle *lib, const char *name)
{ return ffi_symbol(lib, name); }
ETK_DEF etk_sym_handle etk_lib_sym_optional(const etk_lib_handle *lib,
                                            const char *name)
{ return ffi_symbol_optional(lib, name); }
ETK_DEF etk_bool etk_lib_path_search(const char *name, char *out, size_t cap)
{
    size_t n;
    if (name == NULL || out == NULL || cap == 0) return ETK_FALSE;
    n = strlen(name);
    if (n + 1 > cap) return ETK_FALSE;
    memcpy(out, name, n + 1);
    return ETK_TRUE;
}
#endif
#endif
