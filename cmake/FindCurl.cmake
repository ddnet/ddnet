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

is_bundled(IS_BUNDLED "${CURL_LIBRARY}")

set(CURL_LIBRARIES ${CURL_LIBRARY})
set(CURL_INCLUDE_DIRS ${CURL_INCLUDEDIR})
if(IS_BUNDLED AND TARGET_OS STREQUAL "linux")
  find_library(CURL_LIBRARY_SSL
    NAMES ssl
    HINTS ${EXTRA_CURL_LIBDIR}
  )
  find_library(CURL_LIBRARY_CRYPTO
    NAMES crypto
    HINTS ${EXTRA_CURL_LIBDIR}
  )
  # If we don't add `dl`, we get a missing reference to `dlclose`:
  # ```
  # /usr/bin/ld: ../ddnet-libs/curl/linux/lib64/libcrypto.a(dso_dlfcn.o): undefined reference to symbol 'dlclose@@GLIBC_2.2.5'
  # ```
  #
  # Order matters, SSL needs to be linked before CRYPTO, otherwise we also get
  # undefined symbols.
  list(APPEND CURL_LIBRARIES ${CURL_LIBRARY_SSL} ${CURL_LIBRARY_CRYPTO} dl)
endif()

if(IS_BUNDLED AND TARGET_OS STREQUAL "windows")
  set(CURL_COPY_FILES
    "${EXTRA_CURL_LIBDIR}/libcurl.dll"
  )
else()
  set(CURL_COPY_FILES)
endif()
