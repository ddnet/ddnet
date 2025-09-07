if(NOT CMAKE_CROSSCOMPILING)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_SDL3 sdl3)
endif()

set_extra_dirs_lib(SDL3 sdl)
find_library(SDL3_LIBRARY
  NAMES SDL3
  HINTS ${HINTS_SDL3_LIBDIR} ${PC_SDL3_LIBDIR} ${PC_SDL3_LIBRARY_DIRS}
  PATHS ${PATHS_SDL3_LIBDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
if(PREFER_BUNDLED_LIBS)
  set(CMAKE_FIND_FRAMEWORK FIRST)
else()
  set(CMAKE_FIND_FRAMEWORK LAST)
endif()
set_extra_dirs_include(SDL3 sdl "${SDL3_LIBRARY}")
# Looking for 'SDL.h' directly might accidentally find a SDL instead of SDL 2
# installation. Look for a header file only present in SDL 2 instead.
find_path(SDL3_INCLUDEDIR SDL_assert.h
  PATH_SUFFIXES SDL3
  HINTS ${HINTS_SDL3_INCLUDEDIR} ${PC_SDL3_INCLUDEDIR} ${PC_SDL3_INCLUDE_DIRS}
  PATHS ${PATHS_SDL3_INCLUDEDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL3 DEFAULT_MSG SDL3_LIBRARY SDL3_INCLUDEDIR)

mark_as_advanced(SDL3_LIBRARY SDL3_INCLUDEDIR)

if(SDL3_FOUND)
  set(SDL3_LIBRARIES ${SDL3_LIBRARY})
  set(SDL3_INCLUDE_DIRS ${SDL3_INCLUDEDIR})

  is_bundled(SDL3_BUNDLED "${SDL3_LIBRARY}")
  set(SDL3_COPY_FILES)
  set(SDL3_COPY_DIRS)
  if(SDL3_BUNDLED)
    if(TARGET_OS STREQUAL "windows")
      set(SDL3_COPY_FILES "${EXTRA_SDL3_LIBDIR}/SDL3.dll")
      if(TARGET_BITS EQUAL 32)
        list(APPEND OPUSFILE_COPY_FILES
          "${EXTRA_SDL3_LIBDIR}/libgcc_s_dw2-1.dll"
        )
      endif()
    elseif(TARGET_OS STREQUAL "mac")
      set(SDL3_COPY_DIRS "${EXTRA_SDL3_LIBDIR}/SDL3.framework")
    endif()
  endif()
endif()
