if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(PNG)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(PNG_FOUND)
    set(PNG_BUNDLED OFF)
    set(PNG_DEP)
  endif()
endif()

if(NOT PNG_FOUND)
  set_extra_dirs_lib(PNG png)
  find_library(PNG_LIBRARY
    NAMES libpng16.16 png16.16 libpng16-16 png16-16 libpng16 png16
    HINTS ${HINTS_PNG_LIBDIR} ${PC_PNG_LIBDIR} ${PC_PNG_LIBRARY_DIRS}
    PATHS ${PATHS_PNG_LIBDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  set_extra_dirs_include(PNG png "${PNG_LIBRARY}")
  find_path(PNG_INCLUDEDIR
    NAMES png.h
    HINTS ${HINTS_PNG_INCLUDEDIR} ${PC_PNG_INCLUDEDIR} ${PC_PNG_INCLUDE_DIRS}
    PATHS ${PATHS_PNG_INCLUDEDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  mark_as_advanced(PNG_LIBRARY PNG_INCLUDEDIR)

  if(PNG_LIBRARY AND PNG_INCLUDEDIR)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(PNG DEFAULT_MSG PNG_LIBRARY PNG_INCLUDEDIR)

    set(PNG_LIBRARIES ${PNG_LIBRARY})
    set(PNG_INCLUDE_DIRS ${PNG_INCLUDEDIR})
  endif()
endif()

set(PNG_COPY_FILES)
if(PNG_FOUND)
  is_bundled(PNG_BUNDLED "${PNG_LIBRARY}")
  if(PNG_BUNDLED)
    if(TARGET_OS STREQUAL "windows")
      set(PNG_COPY_FILES "${EXTRA_PNG_LIBDIR}/libpng16-16.dll")
    elseif(TARGET_OS STREQUAL "mac")
      set(PNG_COPY_FILES "${EXTRA_PNG_LIBDIR}/libpng16.16.dylib")
    endif()
  endif()
endif()
