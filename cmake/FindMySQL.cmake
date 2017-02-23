
find_path(MYSQL_INCLUDE_DIR
  NAMES "mysql.h"
  PATHS
    "/usr/include/mysql"
    "/usr/local/include/mysql"
)

find_library(MYSQL_LIBRARY
  NAMES "mysqlclient" "mysqlclient_r"
  PATHS
    "/usr/lib/mysql"
    "/usr/lib64/mysql"
    "/usr/local/lib/mysql"
    "/usr/local/lib64/mysql"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDE_DIR)

set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})

mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARY)
