if(NOT CMAKE_CROSSCOMPILING)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_VULKAN vulkan)
  if(PC_VULKAN_FOUND)
    set(VULKAN_INCLUDE_DIRS "${PC_VULKAN_INCLUDE_DIRS}")
    set(VULKAN_LIBRARIES "${PC_VULKAN_LIBRARIES}")
    set(VULKAN_FOUND TRUE)
  endif()
endif()

if(NOT VULKAN_FOUND)
  if(TARGET_OS STREQUAL "android")
    find_library(VULKAN_LIBRARIES
      NAMES vulkan
    )

    find_path(
      VULKAN_INCLUDE_DIRS
      NAMES vulkan/vulkan.h
    )
  elseif(TARGET_OS STREQUAL "mac")
    find_library(VULKAN_LIBRARIES
      NAMES MoltenVK
    )

    find_path(
      VULKAN_INCLUDE_DIRS
      NAMES vulkan/vulkan.h
    )
  else()
    set_extra_dirs_lib(VULKAN vulkan)
    find_library(VULKAN_LIBRARIES
      NAMES vulkan vulkan-1
      HINTS ${HINTS_VULKAN_LIBDIR} ${PC_VULKAN_LIBDIR} ${PC_VULKAN_LIBRARY_DIRS}
      PATHS ${PATHS_VULKAN_LIBDIR}
      ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
    )

    set_extra_dirs_include(VULKAN vulkan "${VULKAN_LIBRARIES}")
    find_path(
      VULKAN_INCLUDE_DIRS
      NAMES vulkan/vulkan.h
      HINTS ${HINTS_VULKAN_INCLUDEDIR} ${PC_VULKAN_INCLUDEDIR} ${PC_VULKAN_INCLUDE_DIRS}
      PATHS ${PATHS_VULKAN_INCLUDEDIR}
      ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
    )
  endif()

  if(VULKAN_INCLUDE_DIRS AND VULKAN_LIBRARIES)
    set(VULKAN_FOUND TRUE)
  else(VULKAN_INCLUDE_DIRS AND VULKAN_LIBRARIES)
    set(VULKAN_FOUND FALSE)
  endif(VULKAN_INCLUDE_DIRS AND VULKAN_LIBRARIES)
endif()

if(TARGET_OS STREQUAL "windows")
  is_bundled(VULKAN_BUNDLED "${VULKAN_LIBRARIES}")
  if(VULKAN_BUNDLED)
    set(VULKAN_COPY_FILES "${EXTRA_VULKAN_LIBDIR}/vulkan-1.dll")
  endif()
endif()
