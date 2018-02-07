if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIG_CMAKE_MODULE_PATH})
  find_package(GLEW)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(GLEW_FOUND)
    add_library(glew UNKNOWN IMPORTED)
    set_target_properties(glew PROPERTIES
      IMPORTED_LOCATION "${GLEW_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${GLEW_INCLUDEDIR}"
    )
    set(GLEW_BUNDLED OFF)
  endif()
endif()

if(NOT GLEW_FOUND)
  set_glob(GLEW_SRC GLOB src/engine/external/glew glew.c)
  set_glob(GLEW_INCLUDES GLOB src/engine/external/glew/GL eglew.h glew.h glxew.h wglew.h)
  add_object_library(glew EXCLUDE_FROM_ALL ${GLEW_SRC} ${GLEW_INCLUDES})
  #target_include_directories(glew PRIVATE src/engine/external/glew)
  target_compile_definitions(glew-obj PUBLIC GLEW_STATIC)
  list(APPEND TARGETS_DEP glew)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(GLEW DEFAULT_MSG GLEW_SRC)
  set(GLEW_BUNDLED ON)
endif()
