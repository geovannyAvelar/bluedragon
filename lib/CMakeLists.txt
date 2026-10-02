# libbluedragon: HID feature-report driver for the M711 mouse. BUILD_SHARED_LIBS selects shared/static.
add_library(bluedragon src/bluedragon.c)
add_library(bluedragon::bluedragon ALIAS bluedragon)

target_include_directories(bluedragon
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties(bluedragon PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  C_VISIBILITY_PRESET hidden)

# Components: lib = runtime library (libbluedragon0), dev = everything needed to build against it.
install(TARGETS bluedragon EXPORT bluedragonTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib NAMELINK_COMPONENT dev
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT lib)
install(FILES include/bluedragon.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT dev)
install(EXPORT bluedragonTargets NAMESPACE bluedragon:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bluedragon COMPONENT dev)

include(CMakePackageConfigHelpers)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/bluedragonConfig.cmake
"include(\"\${CMAKE_CURRENT_LIST_DIR}/bluedragonTargets.cmake\")\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/bluedragonConfigVersion.cmake
  COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bluedragonConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/bluedragonConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bluedragon COMPONENT dev)
