# Narrow integration targets keep vendor implementation types private.
add_library(limestone_satie INTERFACE)
add_library(satie::satie ALIAS limestone_satie)
target_include_directories(limestone_satie SYSTEM INTERFACE
  ${PROJECT_SOURCE_DIR}/third_party/satie/include)

add_library(limestone_equinox INTERFACE)
add_library(equinox::equinox-ng ALIAS limestone_equinox)
target_include_directories(limestone_equinox SYSTEM INTERFACE
  ${PROJECT_SOURCE_DIR}/third_party/equinox-ng)
target_compile_definitions(limestone_equinox INTERFACE EQUINOXNG_DISABLE_DSLUTILS)

foreach(component IN ITEMS sexprtk dsltk ekippx)
  add_library(limestone_${component} INTERFACE)
  add_library(metatk::${component} ALIAS limestone_${component})
endforeach()
target_include_directories(limestone_sexprtk SYSTEM INTERFACE
  ${PROJECT_SOURCE_DIR}/third_party/metatk/SExprTk/include)
target_include_directories(limestone_dsltk SYSTEM INTERFACE
  ${PROJECT_SOURCE_DIR}/third_party/metatk/DSLtk)
target_include_directories(limestone_ekippx SYSTEM INTERFACE
  ${PROJECT_SOURCE_DIR}/third_party/metatk/EkippX
  ${PROJECT_SOURCE_DIR}/third_party/metatk
  ${PROJECT_SOURCE_DIR}/third_party/metatk/DSLtk
  ${PROJECT_SOURCE_DIR}/third_party/metatk/third_party/SerdeTk/include)

# Build the supplied GLR generator and runtime without its sample programs.
set(dparser_dir ${PROJECT_SOURCE_DIR}/third_party/dparser)
add_library(limestone_dparse STATIC
  ${dparser_dir}/arg.c ${dparser_dir}/parse.c ${dparser_dir}/scan.c
  ${dparser_dir}/dsymtab.c ${dparser_dir}/util.c ${dparser_dir}/read_binary.c
  ${dparser_dir}/dparse_tree.c ${dparser_dir}/version.c)
add_library(limestone_mkdparse STATIC
  ${dparser_dir}/mkdparse.c ${dparser_dir}/write_tables.c
  ${dparser_dir}/grammar.g.c ${dparser_dir}/gram.c ${dparser_dir}/lex.c
  ${dparser_dir}/lr.c)
foreach(target IN ITEMS limestone_dparse limestone_mkdparse)
  target_include_directories(${target} SYSTEM PUBLIC ${dparser_dir})
  target_compile_definitions(${target} PRIVATE D_MAJOR_VERSION=1 D_MINOR_VERSION=30)
endforeach()
add_executable(limestone-make-dparser ${dparser_dir}/make_dparser.c)
target_link_libraries(limestone-make-dparser PRIVATE limestone_mkdparse limestone_dparse)
function(limestone_grammar name)
  set(output ${PROJECT_BINARY_DIR}/generated/${name}.c)
  add_custom_command(OUTPUT ${output}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/generated
    COMMAND limestone-make-dparser ${PROJECT_SOURCE_DIR}/parsers/${name}.g
      -o ${output} -i ${name}
    DEPENDS limestone-make-dparser ${PROJECT_SOURCE_DIR}/parsers/${name}.g
    VERBATIM)
  set(${name}_parser ${output} PARENT_SCOPE)
endfunction()
limestone_grammar(isa)
limestone_grammar(traceml)

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/exolangtk
  ${PROJECT_BINARY_DIR}/vendor/exolangtk EXCLUDE_FROM_ALL)

option(LIMESTONE_ENABLE_PERSISTENT_CACHE "Enable Bin2Bin's LMDB persistent cache" ON)
if(LIMESTONE_ENABLE_PERSISTENT_CACHE)
  find_path(LMDB_INCLUDE_DIR lmdb.h REQUIRED)
  find_library(LMDB_LIBRARY NAMES lmdb REQUIRED)
  add_library(limestone_lmdb INTERFACE)
  target_include_directories(limestone_lmdb SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/third_party/lmdbxx ${LMDB_INCLUDE_DIR})
  target_link_libraries(limestone_lmdb INTERFACE ${LMDB_LIBRARY})
  target_compile_definitions(limestone_lmdb INTERFACE LIMESTONE_HAS_LMDB)
endif()
