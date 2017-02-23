cmake_minimum_required(VERSION 3.7)
project(DDNet)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(WEBSOCKETS "Enable websockets support")

find_package(Curl)
find_package(Freetype)
find_package(MySQL)
find_package(Opusfile)
find_package(PythonInterp)
find_package(SDL2)
find_package(Threads)
find_package(ZLIB)

# TODO: Complain if packages are not found.

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
generate_source("src/game/generated/protocol.cpp" "network_source")
generate_source("src/game/generated/protocol.h" "network_header")
generate_source("src/game/generated/server_data.cpp" "server_content_source")
generate_source("src/game/generated/server_data.h" "server_content_header")

file(GLOB_RECURSE BASE "src/base/*.c" "src/base/*.cpp" "src/base/*.h")
file(GLOB_RECURSE ENGINE_CLIENT "src/engine/client/*.cpp" "src/engine/client/*.h")
file(GLOB_RECURSE ENGINE_SERVER "src/engine/server/*.cpp" "src/engine/server/*.h")
file(GLOB_RECURSE ENGINE_SHARED "src/engine/shared/*.cpp" "src/engine/shared/*.h") 
file(GLOB_RECURSE GAME_CLIENT "src/game/client/*.cpp" "src/game/client/*.h")
file(GLOB_RECURSE GAME_EDITOR "src/game/editor/*.cpp" "src/game/editor/*.h")
file(GLOB_RECURSE GAME_SERVER "src/game/server/*.cpp" "src/game/server/*.h")
file(GLOB GAME_SHARED "src/game/*.cpp" "src/game/*.h")

file(GLOB DEP_MD5 "src/engine/external/md5/*.c" "src/engine/external/md5/*.h")
file(GLOB DEP_JSON "src/engine/external/json-parser/*.c" "src/engine/external/json-parser/*.h")
file(GLOB DEP_PNG "src/engine/external/pnglite/*.c" "src/engine/external/pnglite/*.h")
file(GLOB DEP_WAV "src/engine/external/wavpack/*.c" "src/engine/external/wavpack/*.h")

set(GAME_GENERATED_CLIENT "src/game/generated/client_data.cpp" "src/game/generated/client_data.h")
set(GAME_GENERATED_SERVER "src/game/generated/server_data.cpp" "src/game/generated/server_data.h")
set(GAME_GENERATED_SHARED "src/game/generated/protocol.cpp" "src/game/generated/protocol.h" "src/game/generated/nethash.cpp")

set(CLIENT ${ENGINE_CLIENT} ${GAME_CLIENT} ${GAME_EDITOR} ${GAME_GENERATED_CLIENT})
set(SERVER ${ENGINE_SERVER} ${GAME_SERVER} ${GAME_GENERATED_SERVER})
set(DEPS ${DEP_MD5} ${DEP_WEBSOCKETS})
set(DEPS_CLIENT ${DEP_JSON} ${DEP_PNG} ${DEP_WAV})
set(LIBS Threads::Threads ${ZLIB_LIBRARIES})
set(LIBS_SERVER ${MYSQL_LIBRARIES})
set(LIBS_CLIENT
  ${CURL_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${OPUSFILE_LIBRARIES}
  ${SDL2_LIBRARIES}
  GL
  GLU
  X11
)

set(TARGET_SERVER DDNet-Server)
set(TARGET_CLIENT DDNet)
set(TARGET_ENGINE_SHARED DDNet-Engine-Shared)
set(TARGET_GAME_SHARED DDNet-Game-Shared)

add_library(${TARGET_GAME_SHARED} ${GAME_SHARED} ${GAME_GENERATED_SHARED})
add_library(${TARGET_ENGINE_SHARED} ${ENGINE_SHARED} ${BASE} ${DEPS})
add_executable(${TARGET_CLIENT} ${CLIENT} ${DEPS_CLIENT})
add_executable(${TARGET_SERVER} ${SERVER})
target_link_libraries(${TARGET_ENGINE_SHARED} ${LIBS})
target_link_libraries(${TARGET_GAME_SHARED} ${TARGET_ENGINE_SHARED})
target_link_libraries(${TARGET_CLIENT} ${TARGET_GAME_SHARED} ${LIBS_CLIENT})
target_link_libraries(${TARGET_SERVER} ${TARGET_GAME_SHARED} ${LIBS_SERVER})

include_directories(src/)

if(WEBSOCKETS)
  file(GLOB DEP_WEBSOCKETS "src/engine/external/libwebsockets/*.c" "src/engine/external/libwebsockets/*.h")
  add_definitions(-DWEBSOCKETS)
endif()

if(MYSQL_FOUND)
  target_compile_definitions(${TARGET_SERVER} PRIVATE -DCONF_WEBSOCKETS)
  target_include_directories(${TARGET_SERVER} PRIVATE ${MYSQL_INCLUDE_DIRS})
endif()

target_include_directories(${TARGET_CLIENT} PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_include_directories(${TARGET_CLIENT} PRIVATE ${OPUSFILE_INCLUDE_DIRS})
target_include_directories(${TARGET_CLIENT} PRIVATE ${SDL2_INCLUDE_DIRS})
