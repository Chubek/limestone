cmake_minimum_required(VERSION 3.20)
project(EZDox VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EZDOX_BUILD_TESTS "Build EZDox tests" ON)
option(EZDOX_USE_INJA "Use inja template engine for HTML/LaTeX targets" ON)
option(INSTALL_ADDONS "Install EZDox addon files" ON)

# --- Third-party dependencies (vendored copies) ---
# JSON parsing: nlohmann-json (header-only). Provides nlohmann_json::nlohmann_json.
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(NLOHMANN_JSON_TESTS OFF CACHE INTERNAL "")
add_subdirectory(third_party/nlohmann-json EXCLUDE_FROM_ALL)

# YAML parsing: yaml-cpp (compiled). Provides yaml-cpp::yaml-cpp.
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp EXCLUDE_FROM_ALL)

# inja templating reuses the real nlohmann-json target (no embedded duplicate copy).
if(EZDOX_USE_INJA)
  add_library(Inja INTERFACE)
  target_include_directories(Inja INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/inja/include)
  target_link_libraries(Inja INTERFACE nlohmann_json::nlohmann_json)
endif()

# TOML parsing: tomlplusplus (header-only). Provides tomlplusplus::tomlplusplus.
add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tomlplusplus/include)

# JSON Schema validation: valijson (header-only).
add_library(valijson INTERFACE)
target_include_directories(valijson INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/valijson/include)

add_library(ezdox
  src/EzDox.cpp
  stdmarkup/EZDox-Markdown.cpp
  stdmarkup/EZDox-ReStructuredText.cpp
  stdmarkup/EZDox-Docbook.cpp
  stdmarkup/EZDox-XWiki.cpp
  stdtarget/EZDox-HTML.cpp
  stdtarget/EZDox-LaTeX.cpp
  stdtarget/EZDox-Manpage.cpp
  stdtarget/EZDox-ROFF.cpp
  stdtarget/EZDox-XML.cpp
)
target_include_directories(ezdox PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ezdox PUBLIC nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp)
target_link_libraries(ezdox PUBLIC tomlplusplus valijson)
if(EZDOX_USE_INJA)
  target_link_libraries(ezdox PUBLIC Inja)
  target_compile_definitions(ezdox PUBLIC EZDOX_USE_INJA=1)
endif()
add_library(EZDox::ezdox ALIAS ezdox)

add_executable(ezdox-cli
  cli/ezdox-cli.cpp
  cli/ezdox-bundle.cpp
  cli/ezdox-config.cpp
  cli/ezdox-find.cpp
  cli/ezdox-generate.cpp
  cli/ezdox-install.cpp
  cli/ezdox-paths.cpp
)
target_link_libraries(ezdox-cli PRIVATE ezdox)

install(TARGETS ezdox ezdox-cli)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY templates/ DESTINATION share/ezdox/templates)
# A quoted string (not [[...]]) is required here so ${CMAKE_CURRENT_SOURCE_DIR} is
# expanded at *configure* time and baked into the install script as the source path.
# The escaped \$ENV{...} / \${...} stay install-time references so the destination
# follows $EZDOX_HOME / $HOME when the install actually runs.
install(CODE "
  if(DEFINED ENV{EZDOX_HOME} AND NOT \"\$ENV{EZDOX_HOME}\" STREQUAL \"\")
    set(ezdox_manifest_home \"\$ENV{EZDOX_HOME}\")
  else()
    set(ezdox_manifest_home \"\$ENV{HOME}/.ezdox\")
  endif()
  file(MAKE_DIRECTORY \"\${ezdox_manifest_home}/manifests\")
  file(INSTALL
       DESTINATION \"\${ezdox_manifest_home}/manifests\"
       TYPE DIRECTORY
       FILES \"${CMAKE_CURRENT_SOURCE_DIR}/manifests/\")
")

if(INSTALL_ADDONS)
  include(GNUInstallDirs)
  install(FILES addons/cmake/EZDox.cmake DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/Modules")
  install(DIRECTORY addons/fish/ DESTINATION "${CMAKE_INSTALL_DATADIR}/ezdox/addons/fish")
  install(DIRECTORY addons/zsh/ DESTINATION "${CMAKE_INSTALL_DATADIR}/ezdox/addons/zsh")
  install(DIRECTORY addons/vim/ DESTINATION "${CMAKE_INSTALL_DATADIR}/ezdox/addons/vim")
endif()

if(EZDOX_BUILD_TESTS)
  enable_testing()
  add_executable(ezdox-tests tests/ezdox_tests.cpp)
  target_link_libraries(ezdox-tests PRIVATE ezdox)
  add_test(NAME ezdox-tests COMMAND ezdox-tests)
  add_test(NAME cli-version COMMAND ezdox-cli version)
  add_test(NAME cli-help COMMAND ezdox-cli help)
  add_test(NAME cli-paths COMMAND ezdox-cli paths)
  # A global option before the subcommand (the documented form) must be accepted,
  # not rejected as "unknown command". Guards the leading-global pre-scan.
  add_test(NAME cli-leading-global COMMAND ezdox-cli --color never version)
  set_tests_properties(cli-leading-global PROPERTIES PASS_REGULAR_EXPRESSION "EZDox")
endif()
