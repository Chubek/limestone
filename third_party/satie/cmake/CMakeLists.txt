# CMake helper modules — included by the root CMakeLists.txt.
# Installed alongside the package config so downstream projects can reuse
# the Satie warning set.
install (FILES
  SatieWarnings.cmake
  SatieVersion.cmake
  SatieInstall.cmake
  SatieConfig.cmake.in
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Satie)
