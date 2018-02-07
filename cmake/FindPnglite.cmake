if(NOT PREFER_BUNDLED_LIBS)
  if(NOT CMAKE_CROSSCOMPILING)
    find_package(PkgConfig QUIET)
    pkg_check_modules(PC_PNGLITE pnglite)
  endif()

  find_library(PNGLITE_LIBRARY
    NAMES pnglite
    HINTS ${PC_PNGLITE_LIBDIR} ${PC_PNGLITE_LIBRARY_DIRS}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )
  find_path(PNGLITE_INCLUDEDIR
    NAMES pnglite.h
    HINTS ${PC_PNGLITE_INCLUDEDIR} ${PC_PNGLITE_INCLUDE_DIRS}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  mark_as_advanced(PNGLITE_LIBRARY PNGLITE_INCLUDEDIR)

  if(PNGLITE_LIBRARY AND PNGLITE_INCLUDEDIR)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Pnglite DEFAULT_MSG PNGLITE_LIBRARY PNGLITE_INCLUDEDIR)

    set(PNGLITE_LIBRARIES ${PNGLITE_LIBRARY})
    set(PNGLITE_INCLUDE_DIRS ${PNGLITE_INCLUDEDIR})

    add_library(pnglite UNKNOWN IMPORTED)
    set_target_properties(pnglite PROPERTIES
      IMPORTED_LOCATION "${PNGLITE_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${PNGLITE_INCLUDEDIR}"
    )
    set(PNGLITE_BUNDLED OFF)
  endif()
endif()

if(NOT PNGLITE_FOUND)
  set_glob(PNGLITE_SRC GLOB src/engine/external/pnglite pnglite.c pnglite.h)
  add_object_library(pnglite EXCLUDE_FROM_ALL ${PNGLITE_SRC})
  list(APPEND TARGETS_DEP pnglite-obj)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Pnglite DEFAULT_MSG PNGLITE_SRC)
  set(PNGLITE_BUNDLED ON)
endif()
