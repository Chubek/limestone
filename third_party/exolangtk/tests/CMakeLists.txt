add_executable(exolangtk-smoke smoke.c)
target_link_libraries(exolangtk-smoke PRIVATE ExolangTk)
target_compile_definitions(exolangtk-smoke PRIVATE
    ITK_PLATFORM_IMPLEMENTATION
    ITK_CTYPES_IMPLEMENTATION
    ITK_LAYOUT_IMPLEMENTATION
    ITK_CALLCONV_IMPLEMENTATION
    ITK_MANGLE_IMPLEMENTATION
    ITK_CDECL_IMPLEMENTATION
    ITK_MARSHAL_IMPLEMENTATION
    ITK_CSTRING_IMPLEMENTATION
    ITK_ERROR_IMPLEMENTATION
    ITK_ALLOC_IMPLEMENTATION
    FFI_LOADER_IMPLEMENTATION
    FFI_CIF_IMPLEMENTATION
    FFI_FRAME_IMPLEMENTATION
    FFI_CALL_IMPLEMENTATION
    FFI_TRAMPOLINE_IMPLEMENTATION
    FFI_CLOSURE_IMPLEMENTATION
    FFI_LIBRARY_IMPLEMENTATION
    DTK_SYM_IMPLEMENTATION
    DTK_UNWIND_IMPLEMENTATION
    DTK_STACK_IMPLEMENTATION
    DTK_BREAKPOINT_IMPLEMENTATION
    ETK_DYNLOAD_IMPLEMENTATION
    ETK_VERSION_IMPLEMENTATION
    ETK_REGISTRY_IMPLEMENTATION
    ETK_API_IMPLEMENTATION)
target_link_libraries(exolangtk-smoke PRIVATE ${CMAKE_DL_LIBS})
add_test(NAME exolangtk-smoke COMMAND exolangtk-smoke)

add_executable(exolangtk-extension extension.c)
target_link_libraries(exolangtk-extension PRIVATE ExolangTk)
add_test(NAME exolangtk-extension COMMAND exolangtk-extension)

add_executable(exolangtk-platform platform.c)
target_link_libraries(exolangtk-platform PRIVATE ExolangTk::InteropTk)
add_test(NAME exolangtk-platform COMMAND exolangtk-platform)

add_executable(exolangtk-version-linkage version_impl.c version_consumer.c)
target_link_libraries(exolangtk-version-linkage PRIVATE ExolangTk::ExtensionTk)
add_test(NAME exolangtk-version-linkage COMMAND exolangtk-version-linkage)
