add_library(ExolangTk_FFItk INTERFACE)
add_library(ExolangTk::FFItk ALIAS ExolangTk_FFItk)
target_include_directories(ExolangTk_FFItk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(ExolangTk_FFItk INTERFACE ExolangTk_InteropTk)
target_link_libraries(ExolangTk INTERFACE ExolangTk_FFItk)
set_target_properties(ExolangTk_FFItk PROPERTIES EXPORT_NAME FFItk)
target_compile_features(ExolangTk_FFItk INTERFACE c_std_99)
target_link_libraries(ExolangTk_FFItk INTERFACE ${CMAKE_DL_LIBS})
