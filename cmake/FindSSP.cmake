if(TARGET_OS STREQUAL "windows")
  set_extra_dirs_lib(SSP ssp)
  find_file(SSP_LIBRARY
    NAMES libssp-0.dll
    HINTS ${HINTS_SSP_LIBDIR}
    PATHS ${PATHS_SSP_LIBDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  is_bundled(SSP_BUNDLED "${SSP_LIBRARY}")
  if(NOT SSP_BUNDLED)
    message(FATAL_ERROR "could not find ssp paths")
  endif()
  set(SSP_COPY_FILES
    "${EXTRA_SSP_LIBDIR}/libssp-0.dll"
  )
endif()
