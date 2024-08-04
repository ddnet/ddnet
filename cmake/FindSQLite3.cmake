if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(SQLite3)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
endif()

if(NOT CMAKE_CROSSCOMPILING)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_SQLite3 sqlite3)
endif()

set_extra_dirs_lib(SQLite3 sqlite3)
find_library(SQLite3_LIBRARY
  NAMES sqlite3 sqlite3.0
  HINTS ${HINTS_SQLite3_LIBDIR} ${PC_SQLite3_LIBDIR} ${PC_SQLite3_LIBRARY_DIRS}
  PATHS ${PATHS_SQLite3_LIBDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
set_extra_dirs_include(SQLite3 sqlite3 "${SQLite3_LIBRARY}")
find_path(SQLite3_INCLUDEDIR sqlite3.h
  PATH_SUFFIXES sqlite3
  HINTS ${HINTS_SQLite3_INCLUDEDIR} ${PC_SQLite3_INCLUDEDIR} ${PC_SQLite3_INCLUDE_DIRS}
  PATHS ${PATHS_SQLite3_INCLUDEDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3 DEFAULT_MSG SQLite3_INCLUDEDIR SQLite3_LIBRARY)
mark_as_advanced(SQLite3_INCLUDEDIR SQLite3_LIBRARY)

if(CMAKE_VERSION VERSION_LESS 3.0 AND SQLITE3_FOUND)
  set(SQLite3_FOUND TRUE)
endif()

if(SQLite3_FOUND)
  is_bundled(SQLite3_BUNDLED "${SQLite3_LIBRARY}")
  set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
  set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDEDIR})

  is_bundled(SQLite3_BUNDLED "${SQLite3_LIBRARY}")
  set(SQLite3_COPY_FILES)
  if(SQLite3_BUNDLED AND TARGET_OS STREQUAL "windows")
    if (TARGET_CPU_ARCHITECTURE STREQUAL "arm64")
      set(SQLite3_COPY_FILES
        "${EXTRA_SQLite3_LIBDIR}/libsqlite3-0.dll"
      )
    else()
      set(SQLite3_COPY_FILES "${EXTRA_SQLite3_LIBDIR}/sqlite3.dll")
    endif()
  endif()
endif()
