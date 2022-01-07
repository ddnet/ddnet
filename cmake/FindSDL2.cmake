if(NOT CMAKE_CROSSCOMPILING)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_SDL2 sdl2)
endif()

set_extra_dirs_lib(SDL2 sdl)
find_library(SDL2_LIBRARY
  NAMES SDL2
  HINTS ${HINTS_SDL2_LIBDIR} ${PC_SDL2_LIBDIR} ${PC_SDL2_LIBRARY_DIRS}
  PATHS ${PATHS_SDL2_LIBDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
if(PREFER_BUNDLED_LIBS)
  set(CMAKE_FIND_FRAMEWORK FIRST)
else()
  set(CMAKE_FIND_FRAMEWORK LAST)
endif()
set_extra_dirs_include(SDL2 sdl "${SDL2_LIBRARY}")
# Looking for 'SDL.h' directly might accidentally find a SDL instead of SDL 2
# installation. Look for a header file only present in SDL 2 instead.
find_path(SDL2_INCLUDEDIR SDL_assert.h
  PATH_SUFFIXES SDL2
  HINTS ${HINTS_SDL2_INCLUDEDIR} ${PC_SDL2_INCLUDEDIR} ${PC_SDL2_INCLUDE_DIRS}
  PATHS ${PATHS_SDL2_INCLUDEDIR}
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2 DEFAULT_MSG SDL2_LIBRARY SDL2_INCLUDEDIR)

mark_as_advanced(SDL2_LIBRARY SDL2_INCLUDEDIR)

if(SDL2_FOUND)
  set(SDL2_LIBRARIES ${SDL2_LIBRARY})
  set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDEDIR})

  is_bundled(SDL2_BUNDLED "${SDL2_LIBRARY}")
  set(SDL2_COPY_FILES)
  set(SDL2_COPY_DIRS)
  if(SDL2_BUNDLED)
    if(TARGET_OS STREQUAL "windows")
      set(SDL2_COPY_FILES "${EXTRA_SDL2_LIBDIR}/SDL2.dll")
      if(TARGET_BITS EQUAL 32)
        list(APPEND OPUSFILE_COPY_FILES
          "${EXTRA_SDL2_LIBDIR}/libgcc_s_dw2-1.dll"
        )
      endif()
    elseif(TARGET_OS STREQUAL "mac")
      set(SDL2_COPY_DIRS "${EXTRA_SDL2_LIBDIR}/SDL2.framework")
    endif()
  endif()
endif()
