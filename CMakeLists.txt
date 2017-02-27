cmake_minimum_required(VERSION 2.8)
project(DDNet)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(WEBSOCKETS "Enable websockets support" OFF)
option(MYSQL "Enable mysql support" ON)
option(CLIENT "Compile client" ON)
option(SERVER "Compile server" ON)

# Set the default build type to Release
if(NOT(CMAKE_BUILD_TYPE))
  set(CMAKE_BUILD_TYPE Release)
endif()

########################################################################
# DEPENDENCIES
########################################################################

find_package(Curl)
find_package(Freetype)
if(MYSQL)
  find_package(MySQL)
endif()
find_package(Opusfile)
find_package(PythonInterp)
find_package(SDL2)
find_package(Threads)
find_package(ZLIB)

message(STATUS "******** DDNet ********")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")

message(STATUS ${name} "Dependencies:")
function(show_dependency_status name varFound)
  if(${varFound})
    message(STATUS " * " ${name} " found")
  else()
    message(STATUS " * " ${name} " not found")
  endif()
endfunction()

show_dependency_status("Curl" "${CURL_FOUND}")
show_dependency_status("Freetype" "${FREETYPE_FOUND}")
if(MYSQL)
  show_dependency_status("MySQL" "${MYSQL_FOUND}")
endif()
show_dependency_status("Opusfile" "${OPUSFILE_FOUND}")
show_dependency_status("PythonInterp" "${PYTHONINTERP_FOUND}")
show_dependency_status("SDL2" "${SDL2_FOUND}")
show_dependency_status("Zlib" "${ZLIB_FOUND}")

if(MYSQL AND NOT(MYSQL_FOUND))
  message(STATUS " ! Warning: MySQL support wanted, but not found.")
  message(STATUS "            MySQL support will be disabled.")
  message(STATUS "            If you don't want MySQL support,")
  message(STATUS "            please execute cmake with the option -DMYSQL=OFF.")
  set(MYSQL OFF)
endif()

if(CLIENT AND NOT(CURL_FOUND))
  message(SEND_ERROR "You must install Curl to compile DDNet client")
endif()
if(CLIENT AND NOT(FREETYPE_FOUND))
  message(SEND_ERROR "You must install Freetype to compile DDNet client")
endif()
if(CLIENT AND NOT(OPUSFILE_FOUND))
  message(SEND_ERROR "You must install Openfile to compile DDNet client")
endif()
if(CLIENT AND NOT(SDL2_FOUND))
  message(SEND_ERROR "You must install SDL2 to compile DDNet client")
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

#Sources
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
set(TARGET_ENGINE_SHARED DDNet-Engine-Shared)
add_library(${TARGET_ENGINE_SHARED} ${ENGINE_SHARED} ${BASE} ${DEPS})
target_link_libraries(${TARGET_ENGINE_SHARED} ${LIBS})

set(TARGET_GAME_SHARED DDNet-Game-Shared)
add_library(${TARGET_GAME_SHARED} ${GAME_SHARED} ${GAME_GENERATED_SHARED})
target_link_libraries(${TARGET_GAME_SHARED} ${TARGET_ENGINE_SHARED})


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
  file(GLOB DEP_JSON "src/engine/external/json-parser/*.c" "src/engine/external/json-parser/*.h")
  file(GLOB DEP_PNG "src/engine/external/pnglite/*.c" "src/engine/external/pnglite/*.h")
  file(GLOB DEP_WAV "src/engine/external/wavpack/*.c" "src/engine/external/wavpack/*.h")
  set(DEPS_CLIENT ${DEP_JSON} ${DEP_PNG} ${DEP_WAV})

  # Libraries
  set(LIBS_CLIENT
    ${CURL_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${OPUSFILE_LIBRARIES}
    ${SDL2_LIBRARIES}
    GL
    GLU
    X11
  )

  # Target
  set(TARGET_CLIENT DDNet)
  add_executable(${TARGET_CLIENT} ${CLIENT_SRC} ${DEPS_CLIENT})
  target_link_libraries(${TARGET_CLIENT} ${TARGET_GAME_SHARED} ${LIBS_CLIENT})

  target_include_directories(${TARGET_CLIENT} PRIVATE ${FREETYPE_INCLUDE_DIRS})
  target_include_directories(${TARGET_CLIENT} PRIVATE ${OPUSFILE_INCLUDE_DIRS})
  target_include_directories(${TARGET_CLIENT} PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

########################################################################
# SERVER
########################################################################

if(SERVER)
  # Sources
  file(GLOB_RECURSE ENGINE_SERVER "src/engine/server/*.cpp" "src/engine/server/*.h")
  file(GLOB_RECURSE GAME_SERVER "src/game/server/*.cpp" "src/game/server/*.h")
  set(GAME_GENERATED_SERVER "src/game/generated/server_data.cpp" "src/game/generated/server_data.h")
  set(SERVER_SRC ${ENGINE_SERVER} ${GAME_SERVER} ${GAME_GENERATED_SERVER})

  # Libraries
  set(LIBS_SERVER ${MYSQL_LIBRARIES})

  # Target
  set(TARGET_SERVER DDNet-Server)
  add_executable(${TARGET_SERVER} ${SERVER_SRC})
  target_link_libraries(${TARGET_SERVER} ${TARGET_GAME_SHARED} ${LIBS_SERVER})

  if(MYSQL_FOUND)
    target_compile_definitions(${TARGET_SERVER} PRIVATE -DCONF_WEBSOCKETS)
    target_include_directories(${TARGET_SERVER} PRIVATE ${MYSQL_INCLUDE_DIRS})
  endif()
endif()
