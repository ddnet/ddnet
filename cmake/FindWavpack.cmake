if(NOT PREFER_BUNDLED_LIBS)
  if(NOT CMAKE_CROSSCOMPILING)
    find_package(PkgConfig QUIET)
    pkg_check_modules(PC_WAVPACK wavpack)
  endif()

  find_library(WAVPACK_LIBRARY
    NAMES wavpack
    HINTS ${PC_WAVPACK_LIBDIR} ${PC_WAVPACK_LIBRARY_DIRS}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )
  find_path(WAVPACK_INCLUDEDIR
    NAMES wavpack.h
    PATH_SUFFIXES wavpack
    HINTS ${PC_WAVPACK_INCLUDEDIR} ${PC_WAVPACK_INCLUDE_DIRS}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  mark_as_advanced(WAVPACK_LIBRARY WAVPACK_INCLUDEDIR)

  if(WAVPACK_LIBRARY AND WAVPACK_INCLUDEDIR)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Wavpack DEFAULT_MSG WAVPACK_LIBRARY WAVPACK_INCLUDEDIR)

    set(WAVPACK_LIBRARIES ${WAVPACK_LIBRARY})
    set(WAVPACK_INCLUDE_DIRS ${WAVPACK_INCLUDEDIR})

    add_library(wavpack UNKNOWN IMPORTED)
    set_target_properties(wavpack PROPERTIES
      IMPORTED_LOCATION "${WAVPACK_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${WAVPACK_INCLUDEDIR}"
    )
    set(WAVPACK_BUNDLED OFF)
  endif()
endif()

if(NOT WAVPACK_FOUND)
  set_glob(WAVPACK_SRC GLOB src/engine/external/wavpack
    bits.c
    float.c
    metadata.c
    unpack.c
    wavpack.h
    words.c
    wputils.c
  )
  add_object_library(wavpack EXCLUDE_FROM_ALL ${WAVPACK_SRC})
  set(WAVPACK_INCLUDEDIR src/engine/external/wavpack)
  set(WAVPACK_INCLUDE_DIRS ${WAVPACK_INCLUDEDIR})
  target_include_directories(wavpack INTERFACE ${WAVPACK_INCLUDEDIR})

  list(APPEND TARGETS_DEP wavpack)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Wavpack DEFAULT_MSG WAVPACK_INCLUDEDIR)
  set(WAVPACK_BUNDLED ON)
endif()

set(TMP ${CMAKE_REQUIRED_LIBRARIES})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} wavpack)
check_symbol_exists(WavpackOpenFileInputEx64 wavpack.h WAVPACK_OPEN_FILE_INPUT_EX64)
set(CMAKE_REQUIRED_LIBRARIES ${TMP})

if(WAVPACK_OPEN_FILE_INPUT_EX64)
  set_target_properties(wavpack PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS CONF_WAVPACK_OPEN_FILE_INPUT_EX64
  )
endif()
