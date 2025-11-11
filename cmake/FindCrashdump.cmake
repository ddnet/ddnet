if(TARGET_OS STREQUAL "windows")
  set_extra_dirs_lib(CRASHDUMP drmingw)
  find_file(CRASHDUMP_LIBRARY
    NAMES exchndl.dll
    HINTS ${HINTS_CRASHDUMP_LIBDIR}
    PATHS ${PATHS_CRASHDUMP_LIBDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  is_bundled(CRASHDUMP_BUNDLED "${CRASHDUMP_LIBRARY}")
  if(NOT CRASHDUMP_BUNDLED)
    message(FATAL_ERROR "could not find exception handling paths")
  endif()
  if(TARGET_CPU_ARCHITECTURE STREQUAL "arm64")
    set(CRASHDUMP_COPY_FILES
      "${EXTRA_CRASHDUMP_LIBDIR}/exchndl.dll"
      "${EXTRA_CRASHDUMP_LIBDIR}/mgwhelp.dll"
    )
  else()
    set(CRASHDUMP_COPY_FILES
      "${EXTRA_CRASHDUMP_LIBDIR}/exchndl.dll"
      "${EXTRA_CRASHDUMP_LIBDIR}/dbgcore.dll"
      "${EXTRA_CRASHDUMP_LIBDIR}/dbghelp.dll"
      "${EXTRA_CRASHDUMP_LIBDIR}/mgwhelp.dll"
      "${EXTRA_CRASHDUMP_LIBDIR}/symsrv.dll"
    )
  endif()
endif()
