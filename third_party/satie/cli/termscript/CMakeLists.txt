add_library(satie_termscript STATIC Satie.c)
set_target_properties(satie_termscript PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
install(TARGETS satie_termscript ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
