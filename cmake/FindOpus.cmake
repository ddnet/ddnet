find_package(PkgConfig QUIET)
pkg_check_modules(PC_OPUS opus)
set_extra_dirs(OPUS opus)

find_path(OPUS_INCLUDEDIR opus.h
  PATH_SUFFIXES opus
  HINTS ${PC_OPUS_INCLUDEDIR} ${PC_OPUS_INCLUDE_DIRS}
  PATHS ${EXTRA_OPUS_INCLUDEDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus DEFAULT_MSG OPUS_INCLUDEDIR)

mark_as_advanced(OPUS_INCLUDEDIR)

set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDEDIR})
