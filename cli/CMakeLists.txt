add_executable(m711ctl main.c)
target_link_libraries(m711ctl PRIVATE m711::m711)
install(TARGETS m711ctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
