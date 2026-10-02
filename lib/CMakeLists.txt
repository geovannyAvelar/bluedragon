# libm711: HID feature-report driver for the M711 mouse. BUILD_SHARED_LIBS selects shared/static.
add_library(m711 src/m711.c)
add_library(m711::m711 ALIAS m711)

target_include_directories(m711
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(m711 PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  C_VISIBILITY_PRESET hidden)

install(TARGETS m711 EXPORT m711Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/m711.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT m711Targets NAMESPACE m711:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/m711)

include(CMakePackageConfigHelpers)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/m711Config.cmake
"include(\"\${CMAKE_CURRENT_LIST_DIR}/m711Targets.cmake\")\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/m711ConfigVersion.cmake
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/m711Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/m711ConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/m711)
