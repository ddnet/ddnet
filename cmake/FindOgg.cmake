find_package(PkgConfig QUIET)
pkg_check_modules(PC_OGG ogg)

set_extra_dirs(OGG opus)

find_path(OGG_INCLUDEDIR ogg.h
  PATH_SUFFIXES ogg
  HINTS ${PC_OGG_INCLUDEDIR} ${PC_OGG_INCLUDE_DIRS}
  PATHS ${EXTRA_OGG_INCLUDEDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ogg DEFAULT_MSG OGG_INCLUDEDIR)

mark_as_advanced(OGG_INCLUDEDIR)

set(OGG_INCLUDE_DIRS ${OGG_INCLUDEDIR})
