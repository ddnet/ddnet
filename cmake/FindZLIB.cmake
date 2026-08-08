if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(ZLIB)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(ZLIB_FOUND)
    set(ZLIB_BUNDLED OFF)
    set(ZLIB_DEP)
  endif()
endif()

if(TARGET_OS STREQUAL "emscripten")
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

  set(BUILD_SHARED_LIBS OFF)
  set(ZLIB_COMPAT ON)
  set(ZLIB_ALIASES OFF)
  set(BUILD_TESTING OFF)
  set(WITH_GTEST OFF)
  set(WITH_FUZZERS OFF)
  set(WITH_BENCHMARKS OFF)
  set(WITH_BENCHMARK_APPS OFF)
  set(WITH_NATIVE_INSTRUCTIONS OFF)
  set(SKIP_INSTALL_ALL ON)
  add_subdirectory(src/engine/external/zlib-ng EXCLUDE_FROM_ALL)

  if(NOT MSVC)
    # GCC emits aligned AVX2 stack accesses without realigning the stack in
    # unoptimized builds, crashing on Windows, so always build the bundled
    # zlib-ng with optimizations.
    # See https://github.com/zlib-ng/zlib-ng/issues/1874
    target_compile_options(zlib-ng PRIVATE $<$<CONFIG:Debug>:-O2>)
  endif()

  set(ZLIB_DEP)
  set(ZLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/src/engine/external/zlib-ng")
  set(ZLIB_LIBRARIES zlib-ng)

  list(APPEND TARGETS_DEP zlib-ng)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(ZLIB DEFAULT_MSG ZLIB_INCLUDE_DIRS)
endif()
