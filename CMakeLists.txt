cmake_minimum_required(VERSION 2.8)
project(DDNet)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(WEBSOCKETS "Enable websockets support" OFF)
option(MYSQL "Enable mysql support" OFF)
option(CLIENT "Compile client" ON)

# Set the default build type to Release
if(NOT(CMAKE_BUILD_TYPE))
  set(CMAKE_BUILD_TYPE Release)
endif()

set(SERVER_EXECUTABLE DDNet-Server CACHE STRING "Name of the built server executable")
set(CLIENT_EXECUTABLE DDNet CACHE STRING "Name of the build client executable")

########################################################################
# DEPENDENCIES
########################################################################

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(TARGET_BITS 32)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TARGET_BITS 64)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(TARGET_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(TARGET_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(TARGET_OS "mac")
endif()

function(set_extra_dirs VARIABLE NAME)
  if(TARGET_BITS AND TARGET_OS)
    set("EXTRA_${VARIABLE}_LIBDIR" "other/${NAME}/${TARGET_OS}/lib${TARGET_BITS}/" PARENT_SCOPE)
  endif()
  set("EXTRA_${VARIABLE}_INCLUDEDIR" "other/${NAME}/include/" PARENT_SCOPE)
endfunction()

# Check for PkgConfig once so all the other `find_package` calls can do it
# quietly.
find_package(PkgConfig)
find_package(Curl)
find_package(Freetype)
if(MYSQL)
  find_package(MySQL)
endif()
find_package(Ogg)
find_package(Opus)
find_package(Opusfile)
find_package(PythonInterp)
find_package(SDL2)
find_package(Threads)
find_package(ZLIB)

message(STATUS "******** DDNet ********")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")

message(STATUS "Dependencies:")
function(show_dependency_status NAME FOUND PATH)
  if(FOUND)
    string(FIND "${PATH}" "${CMAKE_SOURCE_DIR}" LOCAL_PATH_POS)
    if(LOCAL_PATH_POS EQUAL 0)
      message(STATUS " * ${NAME} not found (using bundled version)")
    else()
      message(STATUS " * ${NAME} found")
    endif()
  else()
    message(STATUS " * ${NAME} not found")
  endif()
endfunction()

show_dependency_status("Curl" ${CURL_FOUND} "${CURL_LIBRARY}")
show_dependency_status("Freetype" ${FREETYPE_FOUND} "${FREETYPE_LIBRARY}")
if(MYSQL)
  show_dependency_status("MySQL" ${MYSQL_FOUND} "")
endif()
show_dependency_status("Ogg" ${OGG_FOUND} "${OGG_LIBRARY}")
show_dependency_status("Opus" ${OPUS_FOUND} "${OPUS_LIBRARY}")
show_dependency_status("Opusfile" ${OPUSFILE_FOUND} "${OPUSFILE_LIBRARY}")
show_dependency_status("PythonInterp" ${PYTHONINTERP_FOUND} "")
show_dependency_status("SDL2" ${SDL2_FOUND} "${SDL2_LIBRARY}")
if(ZLIB_FOUND)
  message(STATUS " * Zlib found")
else()
  message(STATUS " * Zlib not found (using bundled version)")
endif()

if(MYSQL AND NOT(MYSQL_FOUND))
  message(SEND_ERROR "You must install MySQL to compile the DDNet server with MySQL support")
endif()

if(CLIENT AND NOT(CURL_FOUND))
  message(SEND_ERROR "You must install Curl to compile the DDNet client")
endif()
if(CLIENT AND NOT(FREETYPE_FOUND))
  message(SEND_ERROR "You must install Freetype to compile the DDNet client")
endif()
if(CLIENT AND NOT(OGG_FOUND))
  message(SEND_ERROR "You must install Ogg to compile the DDNet client")
endif()
if(CLIENT AND NOT(OPUS_FOUND))
  message(SEND_ERROR "You must install Opus to compile the DDNet client")
endif()
if(CLIENT AND NOT(OPUSFILE_FOUND))
  message(SEND_ERROR "You must install Opusfile to compile the DDNet client")
endif()
if(CLIENT AND NOT(SDL2_FOUND))
  message(SEND_ERROR "You must install SDL2 to compile the DDNet client")
endif()


if(NOT(ZLIB_FOUND))
  unset(ZLIB_LIBRARIES)
  file(GLOB ZLIB_SRC "src/engine/external/zlib/*.c" "src/engine/external/zlib/*.h")
  add_library(zlib OBJECT ${ZLIB_SRC})
  set(ZLIB $<TARGET_OBJECTS:zlib>)
endif()

########################################################################
# CODE GENERATION
########################################################################

include_directories(src/)

function(chash output_file)
  add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/${output_file}
    COMMAND ${PYTHON_EXECUTABLE} scripts/cmd5.py ${ARGN} > ${output_file}
    DEPENDS scripts/cmd5.py ${ARGN}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endfunction(chash)

function(generate_source output_file script_parameter)
  add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/${output_file}
    COMMAND ${PYTHON_EXECUTABLE} datasrc/compile.py ${script_parameter} > ${output_file}
    DEPENDS
      datasrc/compile.py
      datasrc/content.py
      datasrc/datatypes.py
      datasrc/network.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endfunction(generate_source)

chash("src/game/generated/nethash.cpp"
  "src/engine/shared/protocol.h"
  "src/game/generated/protocol.h"
  "src/game/tuning.h"
  "src/game/gamecore.cpp"
)
generate_source("src/game/generated/client_data.cpp" "client_content_source")
generate_source("src/game/generated/client_data.h" "client_content_header")
generate_source("src/game/generated/protocol.cpp" "network_source")
generate_source("src/game/generated/protocol.h" "network_header")
generate_source("src/game/generated/server_data.cpp" "server_content_source")
generate_source("src/game/generated/server_data.h" "server_content_header")


########################################################################
# SHARED
########################################################################

# Sources
file(GLOB_RECURSE BASE "src/base/*.c" "src/base/*.cpp" "src/base/*.h")
file(GLOB_RECURSE ENGINE_SHARED "src/engine/shared/*.cpp" "src/engine/shared/*.h") 
file(GLOB GAME_SHARED "src/game/*.cpp" "src/game/*.h")
set(GAME_GENERATED_SHARED "src/game/generated/protocol.cpp" "src/game/generated/protocol.h" "src/game/generated/nethash.cpp")

# Static dependencies
file(GLOB DEP_MD5 "src/engine/external/md5/*.c" "src/engine/external/md5/*.h")

if(WEBSOCKETS)
  file(GLOB DEP_WEBSOCKETS "src/engine/external/libwebsockets/*.c" "src/engine/external/libwebsockets/*.h")
  add_definitions(-DWEBSOCKETS)
endif()

set(DEPS ${DEP_MD5} ${DEP_WEBSOCKETS})

# Libraries
set(LIBS ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARIES})

# Targets
add_library(engine-shared OBJECT ${ENGINE_SHARED} ${BASE} ${DEPS})
add_library(game-shared OBJECT ${GAME_SHARED} ${GAME_GENERATED_SHARED})


########################################################################
# CLIENT
########################################################################

if(CLIENT)
  # Sources
  file(GLOB_RECURSE ENGINE_CLIENT "src/engine/client/*.cpp" "src/engine/client/*.h")
  file(GLOB_RECURSE GAME_CLIENT "src/game/client/*.cpp" "src/game/client/*.h")
  file(GLOB_RECURSE GAME_EDITOR "src/game/editor/*.cpp" "src/game/editor/*.h")
  set(GAME_GENERATED_CLIENT "src/game/generated/client_data.cpp" "src/game/generated/client_data.h")
  set(CLIENT_SRC ${ENGINE_CLIENT} ${GAME_CLIENT} ${GAME_EDITOR} ${GAME_GENERATED_CLIENT})

  # Static dependencies
  # TODO: Make add_library(.. OBJECT ..) out of these.
  file(GLOB DEP_JSON "src/engine/external/json-parser/*.c" "src/engine/external/json-parser/*.h")
  file(GLOB DEP_PNG "src/engine/external/pnglite/*.c" "src/engine/external/pnglite/*.h")
  file(GLOB DEP_WAV "src/engine/external/wavpack/*.c" "src/engine/external/wavpack/*.h")
  set(DEPS_CLIENT ${DEP_JSON} ${DEP_PNG} ${DEP_WAV})

  # Libraries
  set(LIBS_CLIENT
    ${LIBS}
    ${CURL_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${OGG_LIBRARIES}
    ${OPUSFILE_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${SDL2_LIBRARIES}
    GL
    GLU
    X11
  )

  # Target
  set(TARGET_CLIENT ${CLIENT_EXECUTABLE})
  add_executable(${TARGET_CLIENT}
    ${CLIENT_SRC}
    ${DEPS_CLIENT}
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
    ${ZLIB}
  )
  target_link_libraries(${TARGET_CLIENT} ${LIBS_CLIENT})

  target_include_directories(${TARGET_CLIENT} PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${OPUSFILE_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
  )
endif()

########################################################################
# SERVER
########################################################################

# Sources
file(GLOB_RECURSE ENGINE_SERVER "src/engine/server/*.cpp" "src/engine/server/*.h")
file(GLOB_RECURSE GAME_SERVER "src/game/server/*.cpp" "src/game/server/*.h")
set(GAME_GENERATED_SERVER "src/game/generated/server_data.cpp" "src/game/generated/server_data.h")
set(SERVER_SRC ${ENGINE_SERVER} ${GAME_SERVER} ${GAME_GENERATED_SERVER})

# Libraries
set(LIBS_SERVER ${LIBS} ${MYSQL_LIBRARIES})

# Target
set(TARGET_SERVER ${SERVER_EXECUTABLE})
add_executable(${TARGET_SERVER}
  ${SERVER_SRC}
  $<TARGET_OBJECTS:engine-shared>
  $<TARGET_OBJECTS:game-shared>
  ${ZLIB}
)
target_link_libraries(${TARGET_SERVER} ${LIBS_SERVER})

if(MYSQL_FOUND)
  target_compile_definitions(${TARGET_SERVER} PRIVATE -DCONF_WEBSOCKETS)
  target_include_directories(${TARGET_SERVER} PRIVATE ${MYSQL_INCLUDE_DIRS})
endif()

########################################################################
# VARIOUS TARGETS
########################################################################

file(GLOB_RECURSE MASTERSRV_SRC "src/mastersrv/*.cpp" "src/mastersrv/*.h")
file(GLOB_RECURSE VERSIONSRV_SRC  "src/versionsrv/*.cpp" "src/versionsrv/*.h")
file(GLOB_RECURSE TWPING_SRC "src/twping/*.cpp" "src/twping/*.h")

set(TARGET_MASTERSRV mastersrv)
set(TARGET_VERSIONSRV versionsrv)
set(TARGET_TWPING twping)

add_executable(${TARGET_MASTERSRV} EXCLUDE_FROM_ALL ${MASTERSRV_SRC} $<TARGET_OBJECTS:engine-shared> ${ZLIB})
add_executable(${TARGET_VERSIONSRV} EXCLUDE_FROM_ALL ${VERSIONSRV_SRC} $<TARGET_OBJECTS:engine-shared> ${ZLIB})
add_executable(${TARGET_TWPING} EXCLUDE_FROM_ALL ${TWPING_SRC} $<TARGET_OBJECTS:engine-shared> ${ZLIB})

target_link_libraries(${TARGET_MASTERSRV} ${LIBS})
target_link_libraries(${TARGET_VERSIONSRV} ${LIBS})
target_link_libraries(${TARGET_TWPING} ${LIBS})

set(TARGETS
  ${TARGET_CLIENT}
  ${TARGET_SERVER}
  ${TARGET_MASTERSRV}
  ${TARGET_VERSIONSRV}
  ${TARGET_TWPING}
)

file(GLOB TOOLS RELATIVE "${CMAKE_SOURCE_DIR}/src/tools/" "src/tools/*.cpp")
foreach(T ${TOOLS})
  string(REGEX REPLACE "\\.cpp$" "" TOOL "${T}")
  add_executable(${TOOL} EXCLUDE_FROM_ALL
    src/tools/${TOOL}.cpp
    $<TARGET_OBJECTS:engine-shared>
    ${ZLIB}
  )
  if(TOOL MATCHES "^(tileset_|dilate$)")
    target_sources(${TOOL} PRIVATE ${DEP_PNG})
  endif()
  if(TOOL MATCHES "^config_")
    target_sources(${TOOL} PRIVATE "src/tools/config_common.h")
  endif()
  target_link_libraries(${TOOL} ${LIBS})
  list(APPEND TARGETS ${TOOL})
endforeach()

add_custom_target(everything DEPENDS ${TARGETS})
