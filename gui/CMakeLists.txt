# bluedragon-gui: GTK4 front end. UI definitions (data/ui/*.ui) and CSS are compiled into a GResource.
pkg_get_variable(GLIB_COMPILE_RESOURCES gio-2.0 glib_compile_resources)

set(GUI_RESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/data/bluedragon.gresource.xml)
file(GLOB GUI_RESOURCE_FILES CONFIGURE_DEPENDS data/*.css data/ui/*.ui)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources.c
  COMMAND ${GLIB_COMPILE_RESOURCES}
          --target=${CMAKE_CURRENT_BINARY_DIR}/resources.c
          --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/data
          --generate-source ${GUI_RESOURCE_XML}
  DEPENDS ${GUI_RESOURCE_XML} ${GUI_RESOURCE_FILES}
  COMMENT "Compiling GUI resources")

add_executable(bluedragon-gui
  src/main.c
  src/bd-actions.c
  src/bd-action-popover.c
  src/bd-action-row.c
  src/bd-dpi-column.c
  src/bd-mouse-view.c
  src/bd-page.c
  src/bd-page-dpi.c
  src/bd-page-general.c
  src/bd-page-info.c
  src/bd-page-light.c
  src/bd-snapshot.c
  src/bd-window.c
  ${CMAKE_CURRENT_BINARY_DIR}/resources.c)

target_include_directories(bluedragon-gui PRIVATE src ${GTK4_INCLUDE_DIRS})
target_link_directories(bluedragon-gui PRIVATE ${GTK4_LIBRARY_DIRS})
target_compile_options(bluedragon-gui PRIVATE ${GTK4_CFLAGS_OTHER})
target_link_libraries(bluedragon-gui PRIVATE bluedragon::bluedragon ${GTK4_LIBRARIES} m)
install(TARGETS bluedragon-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
