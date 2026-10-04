# Vendored dependency wiring for NuPERF.
# Keep this minimal and target-based.

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11/include")
    add_library(nuperf_cli11 INTERFACE)
    target_include_directories(nuperf_cli11 INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11/include")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include")
    add_library(nuperf_sol2 INTERFACE)
    target_include_directories(nuperf_sol2 INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include")
endif()
