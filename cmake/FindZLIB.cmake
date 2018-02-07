if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(ZLIB)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(ZLIB_FOUND)
    add_library(zlib UNKNOWN IMPORTED)
    set_target_properties(zlib PROPERTIES
      IMPORTED_LOCATION "${ZLIB_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDEDIR}"
    )
    set(ZLIB_BUNDLED OFF)
  endif()
endif()

if(NOT ZLIB_FOUND)
  set_glob(ZLIB_SRC GLOB src/engine/external/zlib
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
  add_object_library(zlib EXCLUDE_FROM_ALL ${ZLIB_SRC})
  target_include_directories(zlib INTERFACE src/engine/external/zlib/)

  list(APPEND TARGETS_DEP zlib)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(ZLIB DEFAULT_MSG ZLIB_SRC)
  set(ZLIB_BUNDLED ON)
endif()
