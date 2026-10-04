add_library(ExolangTk_ExtensionTk INTERFACE)
add_library(ExolangTk::ExtensionTk ALIAS ExolangTk_ExtensionTk)
target_include_directories(ExolangTk_ExtensionTk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(ExolangTk_ExtensionTk INTERFACE
    ExolangTk_InteropTk ExolangTk_FFItk)
target_link_libraries(ExolangTk INTERFACE ExolangTk_ExtensionTk)
set_target_properties(ExolangTk_ExtensionTk PROPERTIES EXPORT_NAME ExtensionTk)
target_compile_features(ExolangTk_ExtensionTk INTERFACE c_std_99)
