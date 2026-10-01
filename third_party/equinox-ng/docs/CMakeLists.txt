set(EQUINOXNG_DOC_FILES
    README.md
    01-overview.md
    02-installation-and-build.md
    03-project-architecture.md
    04-cli-usage-eqvsg.md
    05-api-usage-cpp.md
    06-expression-definitions.md
    07-rewrite-rules.md
    08-s-expressions-and-data-model.md
    09-examples-and-workflows.md
    10-testing-validation-and-fuzzing.md
    11-dslutils-and-advanced-extension.md
    12-troubleshooting-and-reference.md
)

add_custom_target(equinoxng-docs ALL
    SOURCES ${EQUINOXNG_DOC_FILES}
)

install(FILES ${EQUINOXNG_DOC_FILES}
        DESTINATION share/doc/equinoxng)
