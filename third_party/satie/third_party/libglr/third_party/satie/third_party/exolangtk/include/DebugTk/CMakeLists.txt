add_library(ExolangTk_DebugTk INTERFACE)
add_library(ExolangTk::DebugTk ALIAS ExolangTk_DebugTk)
target_include_directories(ExolangTk_DebugTk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(ExolangTk_DebugTk INTERFACE ExolangTk_InteropTk)
target_link_libraries(ExolangTk INTERFACE ExolangTk_DebugTk)
set_target_properties(ExolangTk_DebugTk PROPERTIES EXPORT_NAME DebugTk)
target_compile_features(ExolangTk_DebugTk INTERFACE c_std_99)
target_link_libraries(ExolangTk_DebugTk INTERFACE ${CMAKE_DL_LIBS})
