set(GLR_REWRITE_LIBRARY_FILES
    eiminate-ambguity.grl
    eliminate-epsilon-production.grl
    eliminate-useless-symbols.grl
    left-factor.grl
    make-lr-compat.grl
    remove-left-recursion.grl
    remove-unit-productions.grl
)

install(FILES ${GLR_REWRITE_LIBRARY_FILES}
        DESTINATION share/libglr/rewritelib)
