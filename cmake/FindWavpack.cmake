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
    set(WAVPACK_BUNDLED OFF)
  endif()
endif()

if(NOT WAVPACK_FOUND)
  set(WAVPACK_SRC_DIR src/engine/external/wavpack)
  set_src(WAVPACK_SRC GLOB ${WAVPACK_SRC_DIR}
    common_utils.c
    decorr_utils.c
    entropy_utils.c
    open_legacy.c
    open_utils.c
    read_words.c
    tag_utils.c
    tags.c
    unpack.c
    unpack_dsd.c
    unpack_floats.c
    unpack_seek.c
    unpack_utils.c
    wavpack.h
    wavpack_local.h
    wavpack_version.h
  )
  add_library(wavpack EXCLUDE_FROM_ALL OBJECT ${WAVPACK_SRC})
  set(WAVPACK_DEP $<TARGET_OBJECTS:wavpack>)
  set(WAVPACK_INCLUDEDIR ${WAVPACK_SRC_DIR})
  set(WAVPACK_INCLUDE_DIRS ${WAVPACK_INCLUDEDIR})

  list(APPEND TARGETS_DEP wavpack)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Wavpack DEFAULT_MSG WAVPACK_INCLUDEDIR)
  set(WAVPACK_BUNDLED ON)
endif()
