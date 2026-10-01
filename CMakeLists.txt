cmake_minimum_required(VERSION 3.20)
project(Limestone VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

add_library(limestone_foundation INTERFACE)
target_include_directories(limestone_foundation INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(metacode STATIC metacode/metacode.cpp)
target_link_libraries(metacode PUBLIC limestone_foundation)
target_include_directories(metacode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/metacode)

add_library(schedrow STATIC schedrow/schedrow.cpp)
target_link_libraries(schedrow PUBLIC limestone_foundation)
target_include_directories(schedrow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/schedrow)

add_library(regtl STATIC regtl/regtl.cpp)
target_link_libraries(regtl PUBLIC limestone_foundation)
target_include_directories(regtl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/regtl)

add_library(limeburg STATIC limeburg/limeburg.cpp)
target_link_libraries(limeburg PUBLIC limestone_foundation)
target_include_directories(limeburg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/limeburg)

add_library(unisel STATIC unisel/unisel.cpp)
target_link_libraries(unisel PUBLIC limestone_foundation)
target_include_directories(unisel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unisel)

add_library(bin2bin STATIC bin2bin/bin2bin.cpp)
target_link_libraries(bin2bin PUBLIC limestone_foundation)
target_include_directories(bin2bin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/bin2bin)

add_library(tunah STATIC tunah/tunah.cpp)
target_link_libraries(tunah PUBLIC limestone_foundation)
target_include_directories(tunah PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tunah)

add_library(traceml STATIC traceml/traceml.cpp)
target_link_libraries(traceml PUBLIC limestone_foundation)
target_include_directories(traceml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/traceml)

add_library(exolayer STATIC exolayer/exolayer.cpp)
target_include_directories(exolayer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/exolayer)

add_library(limestone_core STATIC limestone/limestone.cpp)
target_link_libraries(limestone_core PUBLIC limestone_foundation unisel)
target_include_directories(limestone_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/limestone)

add_executable(limestone-cli limestone/main.cpp)
target_link_libraries(limestone-cli PRIVATE limestone_core)
enable_testing()
add_executable(limestone-smoke tests/smoke.cpp)
target_link_libraries(limestone-smoke PRIVATE metacode schedrow regtl limeburg unisel bin2bin tunah traceml exolayer limestone_core)
add_test(NAME limestone-smoke COMMAND limestone-smoke)

# MachineIR is a D package; build it when a D compiler is available.
find_program(DUB_EXECUTABLE dub)
if(DUB_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/metacode/machine-ir/dub.sdl")
  add_custom_target(machine-ir-test COMMAND ${DUB_EXECUTABLE} test WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/metacode/machine-ir")
endif()
