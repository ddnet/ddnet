find_package(PkgConfig QUIET)
pkg_check_modules(PC_CURL libcurl)

set_extra_dirs_lib(CURL curl)
find_library(CURL_LIBRARY
  NAMES curl
  HINTS ${HINTS_CURL_LIBDIR} ${PC_CURL_LIBDIR} ${PC_CURL_LIBRARY_DIRS}
  PATHS ${PATHS_CURL_LIBDIR}
)
set_extra_dirs_include(CURL curl "${CURL_LIBRARY}")
find_path(CURL_INCLUDEDIR curl/curl.h
  HINTS ${HINTS_CURL_INCLUDEDIR} ${PC_CURL_INCLUDEDIR} ${PC_CURL_INCLUDE_DIRS}
  PATHS ${PATHS_CURL_INCLUDEDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Curl DEFAULT_MSG CURL_LIBRARY CURL_INCLUDEDIR)

mark_as_advanced(CURL_LIBRARY CURL_INCLUDEDIR)

set(CURL_LIBRARIES ${CURL_LIBRARY})
set(CURL_INCLUDE_DIRS ${CURL_INCLUDEDIR})

string(FIND "${CURL_LIBRARY}" "${PROJECT_SOURCE_DIR}" LOCAL_PATH_POS)
if(LOCAL_PATH_POS EQUAL 0 AND TARGET_OS STREQUAL "windows")
  set(CURL_COPY_FILES
    "${EXTRA_CURL_LIBDIR}/libcurl.dll"
  )
else()
  set(CURL_COPY_FILES)
endif()
