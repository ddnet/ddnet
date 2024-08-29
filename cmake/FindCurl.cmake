if(NOT CMAKE_CROSSCOMPILING)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_CURL libcurl)
endif()

set_extra_dirs_lib(CURL curl)
find_library(CURL_LIBRARY
  NAMES curl
  HINTS ${HINTS_CURL_LIBDIR} ${PC_CURL_LIBDIR} ${PC_CURL_LIBRARY_DIRS}
  PATHS ${PATHS_CURL_LIBDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
set_extra_dirs_include(CURL curl "${CURL_LIBRARY}")
find_path(CURL_INCLUDEDIR curl/curl.h
  HINTS ${HINTS_CURL_INCLUDEDIR} ${PC_CURL_INCLUDEDIR} ${PC_CURL_INCLUDE_DIRS}
  PATHS ${PATHS_CURL_INCLUDEDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Curl DEFAULT_MSG CURL_LIBRARY CURL_INCLUDEDIR)

mark_as_advanced(CURL_LIBRARY CURL_INCLUDEDIR)

if(CURL_FOUND)
  is_bundled(CURL_BUNDLED "${CURL_LIBRARY}")
  set(CURL_LIBRARIES ${CURL_LIBRARY})
  set(CURL_INCLUDE_DIRS ${CURL_INCLUDEDIR})
  if (CURL_BUNDLED AND TARGET_OS STREQUAL "windows" AND TARGET_CPU_ARCHITECTURE STREQUAL "arm64")
    set(CURL_COPY_FILES
      "${EXTRA_CURL_LIBDIR}/libcurl-4.dll"
    )
  elseif(CURL_BUNDLED AND TARGET_OS STREQUAL "windows")
    set(CURL_COPY_FILES
      "${EXTRA_CURL_LIBDIR}/libcurl.dll"
      "${EXTRA_CURL_LIBDIR}/zlib1.dll"
    )
  else()
    set(CURL_COPY_FILES)
  endif()
endif()
