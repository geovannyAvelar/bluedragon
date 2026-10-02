# Debian packages via CPack. Configure with the "deb" preset (prefix /usr, multiarch libdir), then:
#   cmake --build --preset deb --target package
# Components: lib (libbluedragon0), dev (libbluedragon-dev), cli (bluedragon), gui (bluedragon-gui).
set(CPACK_GENERATOR DEB)
# Package version: BD_PACKAGE_VERSION (set by CI: "0.1.0~unstable.<date>.<sha>" or the tag) or the project version.
set(BD_PACKAGE_VERSION "${PROJECT_VERSION}" CACHE STRING "Debian upstream version of the packages")
set(CPACK_PACKAGE_NAME bluedragon)
set(CPACK_PACKAGE_VERSION ${BD_PACKAGE_VERSION})
set(CPACK_PACKAGE_CONTACT "Giovani Avelar <geovanny.avelar@gmail.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/geovannyAvelar/bluedragon")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/COPYING.LESSER)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/packages)

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CPACK_COMPONENTS_ALL lib dev cli gui)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS OFF)           # we ship our own shlibs + triggers
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)  # control files 0644, scripts 0755
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(_ver "${BD_PACKAGE_VERSION}-1")

# --- libbluedragon0: the shared library ---
set(CPACK_DEBIAN_LIB_PACKAGE_NAME libbluedragon0)
set(CPACK_DEBIAN_LIB_PACKAGE_SECTION libs)
set(CPACK_DEBIAN_LIB_DESCRIPTION "Driver library for the M711 gaming mouse
Reads and writes DPI, polling rate, button actions and LED settings of the
M711 mouse (USB 04d9:fc30) over hidraw, without the vendor's Windows tool.")
# control scripts must be named exactly triggers / shlibs / postinst / postrm
configure_file(${CMAKE_SOURCE_DIR}/packaging/deb/lib/shlibs.in ${CMAKE_BINARY_DIR}/deb-lib/shlibs @ONLY)
file(COPY ${CMAKE_SOURCE_DIR}/packaging/deb/lib/triggers DESTINATION ${CMAKE_BINARY_DIR}/deb-lib)
set(CPACK_DEBIAN_LIB_PACKAGE_CONTROL_EXTRA ${CMAKE_BINARY_DIR}/deb-lib/triggers ${CMAKE_BINARY_DIR}/deb-lib/shlibs)

# --- libbluedragon-dev: header, linker symlink, CMake package ---
set(CPACK_DEBIAN_DEV_PACKAGE_NAME libbluedragon-dev)
set(CPACK_DEBIAN_DEV_PACKAGE_SECTION libdevel)
set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "libbluedragon0 (= ${_ver})")
set(CPACK_DEBIAN_DEV_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_DEV_DESCRIPTION "Development files for libbluedragon
Header and CMake package for building programs that configure the M711
gaming mouse.")

# --- bluedragon: command line tool + udev rule ---
set(CPACK_DEBIAN_CLI_PACKAGE_NAME bluedragon)
set(CPACK_DEBIAN_CLI_PACKAGE_DEPENDS "libbluedragon0 (= ${_ver}), libc6")
set(CPACK_DEBIAN_CLI_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_CLI_PACKAGE_RECOMMENDS bluedragon-gui)
set(CPACK_DEBIAN_CLI_DESCRIPTION "Command line tool for the M711 gaming mouse
Dump and change DPI levels, polling rate, button actions and LED settings
of each of the mouse's five profiles. Installs a udev rule that gives the
logged-in user access to the mouse's hidraw device.")
set(CPACK_DEBIAN_CLI_PACKAGE_CONTROL_EXTRA
    ${CMAKE_SOURCE_DIR}/packaging/deb/cli/postinst ${CMAKE_SOURCE_DIR}/packaging/deb/cli/postrm)

# --- bluedragon-gui: GTK4 front end ---
set(CPACK_DEBIAN_GUI_PACKAGE_NAME bluedragon-gui)
set(CPACK_DEBIAN_GUI_PACKAGE_DEPENDS "bluedragon (= ${_ver}), libbluedragon0 (= ${_ver}), libgtk-4-1 (>= 4.10), librsvg2-common, libc6")
set(CPACK_DEBIAN_GUI_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_GUI_PACKAGE_SECTION gnome)
set(CPACK_DEBIAN_GUI_DESCRIPTION "GTK4 settings window for the M711 gaming mouse
Graphical front end to change DPI stages, button actions, lighting and
polling rate of the M711 mouse profiles.")

# Debian policy files for every package: copyright and a gzipped changelog.
string(TIMESTAMP BD_CHANGELOG_DATE "%a, %d %b %Y %H:%M:%S +0000" UTC)
configure_file(${CMAKE_SOURCE_DIR}/packaging/deb/changelog.in ${CMAKE_BINARY_DIR}/changelog @ONLY)
set(_gz ${CMAKE_BINARY_DIR}/changelog.Debian.gz)
execute_process(COMMAND gzip -9nc ${CMAKE_BINARY_DIR}/changelog OUTPUT_FILE ${_gz})
foreach(pair "lib;libbluedragon0" "dev;libbluedragon-dev" "cli;bluedragon" "gui;bluedragon-gui")
  list(GET pair 0 _comp)
  list(GET pair 1 _pkg)
  install(FILES ${CMAKE_SOURCE_DIR}/packaging/deb/copyright
          DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${_pkg} COMPONENT ${_comp})
  install(FILES ${_gz} DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${_pkg} COMPONENT ${_comp})
endforeach()

# Man pages (gzipped), shipped with the package of the program they describe.
foreach(pair "cli;bluedragon" "gui;bluedragon-gui")
  list(GET pair 0 _comp)
  list(GET pair 1 _page)
  set(_man ${CMAKE_BINARY_DIR}/${_page}.1.gz)
  execute_process(COMMAND gzip -9nc ${CMAKE_SOURCE_DIR}/packaging/man/${_page}.1 OUTPUT_FILE ${_man})
  install(FILES ${_man} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT ${_comp})
endforeach()

include(CPack)
