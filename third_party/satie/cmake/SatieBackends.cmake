# Build the vendored parser without its unrelated tools, caches, or nested
# projects. Missing transitive sources are fetched at pinned revisions into
# the build tree; offline builds can supply either source directory below.
include(FetchContent)
find_package(Threads REQUIRED)
set(SATIE_KLIB_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/libglr/third_party/klib"
    CACHE PATH "Directory containing libglr's klib sources")
set(SATIE_CTL_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/libglr/third_party/ctl"
    CACHE PATH "Directory containing libglr's CTL sources")
option(SATIE_FETCH_PARSER_DEPS "Fetch missing libglr transitive dependencies" ON)

function(satie_parser_dependency name source_var required_file repository revision)
  if(EXISTS "${${source_var}}/${required_file}")
    return()
  endif()
  if(NOT SATIE_FETCH_PARSER_DEPS)
    message(FATAL_ERROR "Missing ${name}: set ${source_var} to a complete source directory or enable SATIE_FETCH_PARSER_DEPS")
  endif()
  FetchContent_Declare(${name} GIT_REPOSITORY "${repository}" GIT_TAG "${revision}")
  # These pinned repositories have no top-level CMakeLists.txt, so this only
  # makes their sources available for the satie_glr target below.
  FetchContent_MakeAvailable(${name})
  set(${source_var} "${${name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

satie_parser_dependency(satie_klib SATIE_KLIB_SOURCE_DIR kalloc.c
  https://github.com/attractivechaos/klib.git 97a0fcb790b43b9e5da8994f4671021fec036f19)
satie_parser_dependency(satie_ctl SATIE_CTL_SOURCE_DIR ctl/vec.h
  https://github.com/glouw/ctl.git 435a2e6ba6cc4222e890b4c5993a750d29cb04e3)

# libglr includes klib headers with a prefix; copy headers into a build-only
# include tree to support user-supplied dependency directories on all hosts.
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/backends/include/klib")
file(GLOB klib_headers "${SATIE_KLIB_SOURCE_DIR}/*.h")
foreach(header IN LISTS klib_headers)
  get_filename_component(name "${header}" NAME)
  configure_file("${header}" "${PROJECT_BINARY_DIR}/backends/include/klib/${name}" COPYONLY)
endforeach()
set(HAVE_LMDB 0)
set(HAVE_LIBMDBX 0)
configure_file("${PROJECT_SOURCE_DIR}/third_party/libglr/cmake/config.h.in"
  "${PROJECT_BINARY_DIR}/backends/include/glr/config.h")

set(glr_root "${PROJECT_SOURCE_DIR}/third_party/libglr")
set(glr_sources)
foreach(component grammar stack fork forest reduction graph parser parsetbl
    lrtable disambiguate lexer-hooks reader scannerless select semantic-action
    stringpool thread)
  list(APPEND glr_sources "${glr_root}/src/glr/${component}.c")
endforeach()
add_library(satie_glr STATIC ${glr_sources}
  "${SATIE_KLIB_SOURCE_DIR}/kalloc.c" "${SATIE_KLIB_SOURCE_DIR}/kthread.c")
target_include_directories(satie_glr PRIVATE
  "${glr_root}/include" "${PROJECT_BINARY_DIR}/backends/include"
  "${SATIE_CTL_SOURCE_DIR}" "${SATIE_CTL_SOURCE_DIR}/ctl")
target_compile_features(satie_glr PRIVATE c_std_11)
target_link_libraries(satie_glr PUBLIC Threads::Threads)
set_target_properties(satie_glr PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(satie SYSTEM PRIVATE
  "${glr_root}/include" "${PROJECT_BINARY_DIR}/backends/include"
  "${PROJECT_SOURCE_DIR}/third_party/memtkx/include")
target_link_libraries(satie PRIVATE satie_glr)
install(TARGETS satie_glr EXPORT SatieTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES "${SATIE_KLIB_SOURCE_DIR}/LICENSE.txt"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/licenses RENAME klib.txt)
install(FILES "${SATIE_CTL_SOURCE_DIR}/LICENSE"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/licenses RENAME ctl.txt)
