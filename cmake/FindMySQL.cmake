if(NOT CMAKE_CROSSCOMPILING)
  find_program(MYSQL_CONFIG
    NAMES mysql_config mariadb_config
  )

  if(MYSQL_CONFIG)
    exec_program(${MYSQL_CONFIG}
      ARGS --include
      OUTPUT_VARIABLE MY_TMP
    )

    string(REGEX REPLACE "-I([^ ]*)( .*)?" "\\1" MY_TMP "${MY_TMP}")

    set(MYSQL_CONFIG_INCLUDE_DIR ${MY_TMP} CACHE FILEPATH INTERNAL)

    exec_program(${MYSQL_CONFIG}
      ARGS --libs_r
      OUTPUT_VARIABLE MY_TMP
    )

    set(MYSQL_CONFIG_LIBRARIES "")

    string(REGEX MATCHALL "-l[^ ]*" MYSQL_LIB_LIST "${MY_TMP}")
    foreach(LIB ${MYSQL_LIB_LIST})
      string(REGEX REPLACE "[ ]*-l([^ ]*)" "\\1" LIB "${LIB}")
      list(APPEND MYSQL_CONFIG_LIBRARIES "${LIB}")
    endforeach()

    set(MYSQL_CONFIG_LIBRARY_PATH "")

    string(REGEX MATCHALL "-L[^ ]*" MYSQL_LIBDIR_LIST "${MY_TMP}")
    foreach(LIB ${MYSQL_LIBDIR_LIST})
      string(REGEX REPLACE "[ ]*-L([^ ]*)" "\\1" LIB "${LIB}")
      list(APPEND MYSQL_CONFIG_LIBRARY_PATH "${LIB}")
    endforeach()
  endif()
endif()

set_extra_dirs_lib(MYSQL mysql)

find_library(MYSQL_LIBRARY
  NAMES "mysqlclient" "mysqlclient_r" "mariadbclient"
  #explicitly tell CMake to search through the nix/store when using nix/NixOS
  HINTS 
    ${NIX_STORE_DIR}
  PATHS
    /nix/store
  PATH_SUFFIXES
    mysql
    mariadb
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)
find_path(MYSQL_INCLUDEDIR
  NAMES "mysql.h"
  HINTS 
    ${NIX_STORE_DIR}
  PATHS
    /nix/store
  PATH_SUFFIXES
    mysql
    mariadb
    mysql/mysql
  ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDEDIR)

if(MYSQL_FOUND)
  set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
  set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDEDIR})
  
  if(NOT TARGET MySQL::MySQL)
    add_library(MySQL::MySQL UNKNOWN IMPORTED)
    set_target_properties(MySQL::MySQL PROPERTIES
      IMPORTED_LOCATION "${MYSQL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${MYSQL_INCLUDE_DIRS}"
    )
  endif()
  
  mark_as_advanced(MYSQL_INCLUDEDIR MYSQL_LIBRARY)
endif()
