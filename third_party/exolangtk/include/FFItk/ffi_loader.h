/**
 * @file ffi_loader.h
 * @brief Portable shared-library opening and symbol lookup.
 * @stability stable
 * @depends FFItk::platform, InteropTk::platform, InteropTk::mangle,
 *          InteropTk::error
 */
#ifndef FFI_LOADER_H
#define FFI_LOADER_H
#include "ffi_types.h"
#include "../InteropTk/itk_error.h"

typedef struct ffi_library {
    void *handle;
    itk_status error;
} ffi_library;

FFI_DEF ffi_status ffi_open(const char *path, unsigned flags,
                            ffi_library *out);
FFI_DEF ffi_status ffi_close(ffi_library *lib);
FFI_DEF void *ffi_symbol(const ffi_library *lib, const char *name);
FFI_DEF void *ffi_symbol_optional(const ffi_library *lib, const char *name);

#define FFI_LIB_NOW  0x1u
#define FFI_LIB_LOCAL 0x2u

#ifdef FFI_LOADER_IMPLEMENTATION
#include <string.h>
#if defined(ITK_OS_WINDOWS)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
FFI_DEF ffi_status ffi_open(const char *path, unsigned flags, ffi_library *out)
{
    if (out == NULL || path == NULL) return FFI_EINVAL;
    out->handle = NULL;
    out->error = itk_status_ok();
#if defined(ITK_OS_WINDOWS)
    (void)flags;
    out->handle = (void *)LoadLibraryA(path);
#else
    out->handle = dlopen(path, (flags & FFI_LIB_NOW) ? RTLD_NOW : RTLD_LAZY |
                         ((flags & FFI_LIB_LOCAL) ? RTLD_LOCAL : RTLD_GLOBAL));
#endif
    if (out->handle == NULL) {
        out->error = itk_status_set(ITK_EFAIL, "unable to open library");
        return FFI_EFAIL;
    }
    return FFI_OK;
}
FFI_DEF ffi_status ffi_close(ffi_library *lib)
{
    if (lib == NULL || lib->handle == NULL) return FFI_EINVAL;
#if defined(ITK_OS_WINDOWS)
    if (!FreeLibrary((HMODULE)lib->handle)) return FFI_EFAIL;
#else
    if (dlclose(lib->handle) != 0) return FFI_EFAIL;
#endif
    lib->handle = NULL;
    return FFI_OK;
}
FFI_DEF void *ffi_symbol(const ffi_library *lib, const char *name)
{
    if (lib == NULL || lib->handle == NULL || name == NULL) return NULL;
#if defined(ITK_OS_WINDOWS)
    return (void *)(uintptr_t)GetProcAddress((HMODULE)lib->handle, name);
#else
    return dlsym(lib->handle, name);
#endif
}
FFI_DEF void *ffi_symbol_optional(const ffi_library *lib, const char *name)
{
    return ffi_symbol(lib, name);
}
#endif
#endif
