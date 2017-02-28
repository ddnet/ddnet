find_package(PkgConfig QUIET)
pkg_check_modules(PC_OPUS opus)
set_extra_dirs(OPUS opus)

find_path(OPUS_INCLUDEDIR opus.h
  PATH_SUFFIXES opus
  HINTS ${PC_OPUS_INCLUDEDIR} ${PC_OPUS_INCLUDE_DIRS}
  PATHS ${EXTRA_OPUS_INCLUDEDIR}
)
find_library(OPUS_LIBRARY
  NAMES opus
  HINTS ${PC_OPUS_LIBDIR} ${PC_OPUS_LIBRARY_DIRS}
  PATHS ${EXTRA_OPUS_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus DEFAULT_MSG OPUS_LIBRARY OPUS_INCLUDEDIR)

mark_as_advanced(OPUS_LIBRARY OPUS_INCLUDEDIR)

set(OPUS_LIBRARIES ${OPUS_LIBRARY})
set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDEDIR})
