# SatieInstall.cmake — package configuration for find_package(Satie).
#
# Usage (from the root CMakeLists.txt):
#   include(SatieInstall)
#   satie_install_package_config()
#
# Generates SatieConfig.cmake + SatieConfigVersion.cmake from
# cmake/SatieConfig.cmake.in and installs the exported SatieTargets.

include (CMakePackageConfigHelpers)

function (satie_install_package_config)
  configure_package_config_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SatieConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SatieConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Satie")

  write_basic_package_version_file (
    "${CMAKE_CURRENT_BINARY_DIR}/SatieConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)

  install (FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SatieConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SatieConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Satie")
endfunction ()
