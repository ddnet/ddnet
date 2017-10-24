find_program(MYSQL_CONFIG
  NAMES mysql_config
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
  endforeach(LIB ${MYSQL_LIBS})

  set(MYSQL_CONFIG_LIBRARY_PATH "")

  string(REGEX MATCHALL "-L[^ ]*" MYSQL_LIBDIR_LIST "${MY_TMP}")
  foreach(LIB ${MYSQL_LIBDIR_LIST})
    string(REGEX REPLACE "[ ]*-L([^ ]*)" "\\1" LIB "${LIB}")
    list(APPEND MYSQL_CONFIG_LIBRARY_PATH "${LIB}")
  endforeach(LIB ${MYSQL_LIBS})
endif(MYSQL_CONFIG)

set_extra_dirs_lib(MYSQL mysql)
find_library(MYSQL_LIBRARY
  NAMES "mysqlclient" "mysqlclient_r" "mariadbclient"
  HINTS ${HINTS_MYSQL_LIBDIR} ${MYSQL_CONFIG_LIBRARY_PATH}
  PATHS ${PATHS_MYSQL_LIBDIR}
)
set_extra_dirs_include(MYSQL mysql "${MYSQL_LIBRARY}")
find_path(MYSQL_INCLUDEDIR
  NAMES "mysql.h"
  HINTS ${HINTS_MYSQL_INCLUDEDIR} ${MYSQL_CONFIG_INCLUDE_DIR}
  PATHS ${PATHS_MYSQL_INCLUDEDIR}
)

set_extra_dirs_lib(MYSQL_CPPCONN mysql)
find_library(MYSQL_CPPCONN_LIBRARY
  NAMES "mysqlcppconn" "mysqlcppconn-static"
  HINTS ${HINTS_MYSQL_CPPCONN_LIBDIR} ${MYSQL_CONFIG_LIBRARY_PATH}
  PATHS ${PATHS_MYSQL_CPPCONN_LIBDIR}
)
set_extra_dirs_include(MYSQL_CPPCONN mysql "${MYSQL_CPPCONN_LIBRARY}")
find_path(MYSQL_CPPCONN_INCLUDEDIR
  NAMES "mysql_connection.h"
  HINTS ${HINTS_MYSQL_CPPCONN_INCLUDEDIR} ${MYSQL_CONFIG_INCLUDE_DIR}
  PATHS ${PATHS_MYSQL_CPPCONN_INCLUDEDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDEDIR)

set(MYSQL_LIBRARIES ${MYSQL_LIBRARY} ${MYSQL_CPPCONN_LIBRARY})
set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDEDIR} ${MYSQL_CPPCONN_INCLUDEDIR})

mark_as_advanced(MYSQL_INCLUDEDIR MYSQL_LIBRARY)
