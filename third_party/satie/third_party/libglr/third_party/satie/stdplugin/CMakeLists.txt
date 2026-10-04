add_library(satie_stdplugin INTERFACE)
target_include_directories(satie_stdplugin INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(satie_stdplugin INTERFACE satie::satie)

install(DIRECTORY
  ${CMAKE_CURRENT_SOURCE_DIR}/SemVerValidator
  ${CMAKE_CURRENT_SOURCE_DIR}/DependencySAT
  ${CMAKE_CURRENT_SOURCE_DIR}/PackageResolver
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/satie/stdplugin)
