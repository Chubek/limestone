# SatieVersion.cmake — single source of truth for the library version.
#
# Usage:
#   include(SatieVersion)
#
# Provides SATIE_VERSION_STRING after inclusion. The root project() version
# remains authoritative; this module only exposes it under a stable name
# for subdirectories and packaging scripts.

if (NOT DEFINED SATIE_VERSION_STRING)
  set (SATIE_VERSION_STRING "${PROJECT_VERSION}")
endif ()
