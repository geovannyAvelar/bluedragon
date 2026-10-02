#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "m711::m711" for configuration ""
set_property(TARGET m711::m711 APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(m711::m711 PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libm711.so.0.1.0"
  IMPORTED_SONAME_NOCONFIG "libm711.so.0"
  )

list(APPEND _cmake_import_check_targets m711::m711 )
list(APPEND _cmake_import_check_files_for_m711::m711 "${_IMPORT_PREFIX}/lib/libm711.so.0.1.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
