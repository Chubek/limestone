add_library(ExolangTk_InteropTk INTERFACE)
add_library(ExolangTk::InteropTk ALIAS ExolangTk_InteropTk)
target_include_directories(ExolangTk_InteropTk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(ExolangTk INTERFACE ExolangTk_InteropTk)
set_target_properties(ExolangTk_InteropTk PROPERTIES EXPORT_NAME InteropTk)
target_compile_features(ExolangTk_InteropTk INTERFACE c_std_99)
