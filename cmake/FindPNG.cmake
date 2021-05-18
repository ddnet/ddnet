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
  set(PNG_SRC_DIR src/engine/external/png)
  set_src(PNG_SRC GLOB ${PNG_SRC_DIR}
    png.c
    png.h
    pngconf.h
    pngdebug.h
    pngerror.c
    pngget.c
    pnginfo.h
    pnglibconf.h
    pngmem.c
    pngpread.c
    pngpriv.h
    pngread.c
    pngrio.c
    pngrtran.c
    pngrutil.c
    pngset.c
    pngstruct.h
    pngtrans.c
    pngwio.c
    pngwrite.c
    pngwtran.c
    pngwutil.c
  )
  add_library(png EXCLUDE_FROM_ALL OBJECT ${PNG_SRC})
  target_include_directories(png PRIVATE ${ZLIB_INCLUDE_DIRS})
  set(PNG_DEP $<TARGET_OBJECTS:png>)
  set(PNG_INCLUDEDIR ${PNG_SRC_DIR})
  set(PNG_INCLUDE_DIRS ${PNG_INCLUDEDIR})

  list(APPEND TARGETS_DEP png)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(PNG DEFAULT_MSG PNG_INCLUDEDIR)
  set(PNG_BUNDLED ON)
endif()
