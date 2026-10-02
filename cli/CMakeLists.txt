add_executable(bluedragon-cli main.c)
set_target_properties(bluedragon-cli PROPERTIES OUTPUT_NAME bluedragon)
target_link_libraries(bluedragon-cli PRIVATE bluedragon::bluedragon)
install(TARGETS bluedragon-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT cli)
install(FILES ${CMAKE_SOURCE_DIR}/packaging/udev/70-bluedragon.rules
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d COMPONENT cli)
