if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(ZLIB)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(ZLIB_FOUND)
    set(ZLIB_BUNDLED OFF)
    set(ZLIB_DEP)
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set_extra_dirs_lib(ZLIB zlib)
  find_library(ZLIB_LIBRARY
    NAMES z
    HINTS ${HINTS_ZLIB_LIBDIR} ${PC_ZLIB_LIBDIR} ${PC_ZLIB_LIBRARY_DIRS}
    PATHS ${PATHS_ZLIB_LIBDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )
  set_extra_dirs_include(ZLIB zlib "${ZLIB_LIBRARY}")
  find_path(ZLIB_INCLUDEDIR1 zlib.h
    PATH_SUFFIXES zlib
    HINTS ${HINTS_ZLIB_INCLUDEDIR} ${PC_ZLIB_INCLUDEDIR} ${PC_ZLIB_INCLUDE_DIRS}
    PATHS ${PATHS_ZLIB_INCLUDEDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )
  find_path(ZLIB_INCLUDEDIR2 zconf.h
    PATH_SUFFIXES zlib
    HINTS ${HINTS_ZLIB_INCLUDEDIR} ${PC_ZLIB_INCLUDEDIR} ${PC_ZLIB_INCLUDE_DIRS}
    PATHS ${PATHS_ZLIB_INCLUDEDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  if(ZLIB_LIBRARY AND ZLIB_INCLUDEDIR1 AND ZLIB_INCLUDEDIR2)
    set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDEDIR1} ${ZLIB_INCLUDEDIR2})
    set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
    set(ZLIB_FOUND TRUE)
  endif()
endif()

if(NOT ZLIB_FOUND)
  set(ZLIB_BUNDLED ON)
  set(ZLIB_SRC_DIR src/engine/external/zlib)
  set_src(ZLIB_SRC GLOB ${ZLIB_SRC_DIR}
    adler32.c
    compress.c
    crc32.c
    crc32.h
    deflate.c
    deflate.h
    gzclose.c
    gzguts.h
    gzlib.c
    gzread.c
    gzwrite.c
    infback.c
    inffast.c
    inffast.h
    inffixed.h
    inflate.c
    inflate.h
    inftrees.c
    inftrees.h
    trees.c
    trees.h
    uncompr.c
    zconf.h
    zlib.h
    zutil.c
    zutil.h
  )
  add_library(zlib EXCLUDE_FROM_ALL OBJECT ${ZLIB_SRC})
  set(ZLIB_INCLUDEDIR ${ZLIB_SRC_DIR})
  target_include_directories(zlib PRIVATE ${ZLIB_INCLUDEDIR})

  set(ZLIB_DEP $<TARGET_OBJECTS:zlib>)
  set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDEDIR})
  set(ZLIB_LIBRARIES)

  list(APPEND TARGETS_DEP zlib)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(ZLIB DEFAULT_MSG ZLIB_INCLUDEDIR)
endif()
