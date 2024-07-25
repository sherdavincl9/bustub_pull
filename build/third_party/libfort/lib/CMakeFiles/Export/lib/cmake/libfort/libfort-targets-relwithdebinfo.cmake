#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libfort::fort" for configuration "RelWithDebInfo"
set_property(TARGET libfort::fort APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(libfort::fort PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/libfort.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS libfort::fort )
list(APPEND _IMPORT_CHECK_FILES_FOR_libfort::fort "${_IMPORT_PREFIX}/lib/libfort.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
