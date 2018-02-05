# This module tries to find PngLite library and include files
#
# PNGLITE_INCLUDE_DIR, path where to find pnglite.h
# PNGLITE_LIBRARY_DIR, path where to find pnglite.so
# PNGLITE_LIBRARIES, the library to link against
# PNGLITE_FOUND, If false, do not try to use PngLite
#
# This currently works probably only for Linux

find_path(PNGLITE_INCLUDE_DIR pnglite.h
  /usr/local/include
  /usr/include
)

find_library(PNGLITE_LIBRARIES pnglite
  /usr/local/lib
  /usr/lib
)

get_filename_component(PNGLITE_LIBRARY_DIR ${PNGLITE_LIBRARIES} PATH)

set(PNGLITE_FOUND OFF)
if(PNGLITE_INCLUDE_DIR AND PNGLITE_LIBRARIES)
  set(PNGLITE_FOUND ON)
endif()

mark_as_advanced(
  PNGLITE_LIBRARY_DIR
  PNGLITE_INCLUDE_DIR
  PNGLITE_LIBRARIES
)
