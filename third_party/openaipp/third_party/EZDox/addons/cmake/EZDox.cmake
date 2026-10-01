include_guard(GLOBAL)

set(EZDOX_EXECUTABLE "ezdox-cli" CACHE STRING "EZDox CLI executable")
set(EZDOX_TEMPLATE_DIR "" CACHE PATH "Optional EZDox template directory")

function(ezdox_add_docs target)
  set(options ALL HTML LATEX PDF)
  set(oneValueArgs CONFIG OUTPUT TEMPLATE WORKING_DIRECTORY)
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(EZDOX_DOCS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT EZDOX_DOCS_CONFIG)
    set(EZDOX_DOCS_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/docs/EZDox.yaml")
  endif()
  if(NOT EZDOX_DOCS_OUTPUT)
    set(EZDOX_DOCS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/docs")
  endif()
  if(NOT EZDOX_DOCS_WORKING_DIRECTORY)
    set(EZDOX_DOCS_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  if(NOT EZDOX_DOCS_TEMPLATE AND EZDOX_TEMPLATE_DIR)
    set(EZDOX_DOCS_TEMPLATE "${EZDOX_TEMPLATE_DIR}")
  endif()

  set(target_args)
  if(EZDOX_DOCS_HTML)
    list(APPEND target_args -t HTML)
  endif()
  if(EZDOX_DOCS_LATEX OR EZDOX_DOCS_PDF)
    list(APPEND target_args -t LaTeX)
  endif()
  if(NOT target_args)
    list(APPEND target_args -t HTML -t LaTeX)
  endif()

  set(template_args)
  if(EZDOX_DOCS_TEMPLATE)
    list(APPEND template_args --template "${EZDOX_DOCS_TEMPLATE}")
  endif()

  add_custom_target(${target}
    COMMAND ${EZDOX_EXECUTABLE} generate
            -C "${EZDOX_DOCS_CONFIG}"
            -O "${EZDOX_DOCS_OUTPUT}"
            ${template_args}
            ${target_args}
    DEPENDS ${EZDOX_DOCS_DEPENDS}
    WORKING_DIRECTORY "${EZDOX_DOCS_WORKING_DIRECTORY}"
    COMMENT "Generating EZDox documentation for ${target}"
    VERBATIM)

  if(EZDOX_DOCS_ALL)
    add_custom_target(${target}-all ALL DEPENDS ${target})
  endif()

  if(EZDOX_DOCS_PDF)
    find_program(EZDOX_LATEXMK_EXECUTABLE latexmk)
    if(NOT EZDOX_LATEXMK_EXECUTABLE)
      message(FATAL_ERROR "ezdox_add_docs(${target} PDF) requires latexmk")
    endif()
    add_custom_target(${target}-pdf
      COMMAND "${EZDOX_LATEXMK_EXECUTABLE}" -pdf -interaction=nonstopmode -halt-on-error manual.tex
      DEPENDS ${target}
      WORKING_DIRECTORY "${EZDOX_DOCS_OUTPUT}/latex"
      COMMENT "Building EZDox PDF for ${target}"
      VERBATIM)
  endif()
endfunction()

