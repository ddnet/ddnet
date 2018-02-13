# This module tries to find WavPack library and include files
#
# WAVPACK_INCLUDE_DIR, path where to find wavpack.h
# WAVPACK_LIBRARY_DIR, path where to find wavpack.so
# WAVPACK_LIBRARIES, the library to link against
# WAVPACK_FOUND, If false, do not try to use WavPack
#
# This currently works probably only for Linux

find_path(WAVPACK_INCLUDE_DIR wavpack.h
  /usr/local/include/wavpack
  /usr/local/include
  /usr/include/wavpack
  /usr/include
)

find_library(WAVPACK_LIBRARIES wavpack
  /usr/local/lib
  /usr/lib
)

get_filename_component(WAVPACK_LIBRARY_DIR ${WAVPACK_LIBRARIES} PATH)

set(WAVPACK_FOUND OFF)
if(WAVPACK_INCLUDE_DIR AND WAVPACK_LIBRARIES)
  set(WAVPACK_FOUND ON)
endif()

mark_as_advanced(
  WAVPACK_LIBRARY_DIR
  WAVPACK_INCLUDE_DIR
  WAVPACK_LIBRARIES
)
