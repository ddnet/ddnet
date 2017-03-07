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
    set("EXTRA_${VARIABLE}_LIBDIR" "other/${NAME}/${TARGET_OS}/lib${TARGET_BITS}" PARENT_SCOPE)
  endif()
  set("EXTRA_${VARIABLE}_INCLUDEDIR" "other/${NAME}/include" PARENT_SCOPE)
endfunction()

# Check for PkgConfig once so all the other `find_package` calls can do it
# quietly.
find_package(PkgConfig)
find_package(Curl)
find_package(Freetype)
if(MYSQL)
  find_package(MySQL)
else()
  set(MYSQL_LIBRARIES)
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
    string(FIND "${PATH}" "${PROJECT_SOURCE_DIR}" LOCAL_PATH_POS)
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
show_dependency_status("Ogg" ${OGG_FOUND} "${OGG_INCLUDEDIR}")
show_dependency_status("Opus" ${OPUS_FOUND} "${OPUS_INCLUDEDIR}")
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

if(TARGET_OS STREQUAL "windows")
  set(PLATFORM_CLIENT_LIBS opengl32 glu32 winmm)
  set(PLATFORM_LIBS ws2_32) # Windows sockets
else()
  set(PLATFORM_CLIENT_LIBS GL GLU X11)
  set(PLATFORM_LIBS)
endif()

########################################################################
# INITALIZE TARGET LISTS
########################################################################

set(TARGETS_OWN)
set(TARGETS_DEP)

########################################################################
# DEPENDENCY COMPILATION
########################################################################

if(NOT(ZLIB_FOUND))
  set(ZLIB_LIBRARIES)
  file(GLOB ZLIB_SRC "src/engine/external/zlib/*.c" "src/engine/external/zlib/*.h")
  add_library(zlib EXCLUDE_FROM_ALL OBJECT ${ZLIB_SRC})

  list(APPEND TARGETS_DEP zlib)
  set(ZLIB_INCLUDEDIR src/engine/external/zlib/)
  set(DEP_ZLIB $<TARGET_OBJECTS:zlib>)
else()
  set(ZLIB_INCLUDEDIR)
  set(DEP_ZLIB)
endif()

file(GLOB DEP_PNG_SRC "src/engine/external/pnglite/*.c" "src/engine/external/pnglite/*.h")
add_library(png OBJECT EXCLUDE_FROM_ALL ${DEP_PNG_SRC})
target_include_directories(png PRIVATE ${ZLIB_INCLUDEDIR})

set(DEP_PNG $<TARGET_OBJECTS:png>)
list(APPEND TARGETS_DEP png)

if(CLIENT)
  # Static dependencies
  file(GLOB DEP_JSON_SRC "src/engine/external/json-parser/*.c" "src/engine/external/json-parser/*.h")
  file(GLOB DEP_WAV_SRC "src/engine/external/wavpack/*.c" "src/engine/external/wavpack/*.h")
  add_library(json EXCLUDE_FROM_ALL OBJECT ${DEP_JSON_SRC})
  add_library(wav EXCLUDE_FROM_ALL OBJECT ${DEP_WAV_SRC})

  list(APPEND TARGETS_DEP json wav)
  set(DEP_JSON $<TARGET_OBJECTS:json>)
  set(DEP_WAV $<TARGET_OBJECTS:wav>)
endif()

########################################################################
# COPY DATA AND DLLS
########################################################################

file(COPY data DESTINATION .)
file(COPY ${CURL_COPY_FILES} DESTINATION .)
file(COPY ${FREETYPE_COPY_FILES} DESTINATION .)
file(COPY ${OPUSFILE_COPY_FILES} DESTINATION .)
file(COPY ${SDL2_COPY_FILES} DESTINATION .)

########################################################################
# CODE GENERATION
########################################################################

function(chash output_file)
  add_custom_command(OUTPUT ${PROJECT_SOURCE_DIR}/${output_file}
    COMMAND ${PYTHON_EXECUTABLE} scripts/cmd5.py ${ARGN} > ${output_file}
    DEPENDS scripts/cmd5.py ${ARGN}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endfunction(chash)

function(generate_source output_file script_parameter)
  add_custom_command(OUTPUT ${PROJECT_SOURCE_DIR}/${output_file}
    COMMAND ${PYTHON_EXECUTABLE} datasrc/compile.py ${script_parameter} > ${output_file}
    DEPENDS
      datasrc/compile.py
      datasrc/content.py
      datasrc/datatypes.py
      datasrc/network.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
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
file(GLOB DEP_MD5_SRC "src/engine/external/md5/*.c" "src/engine/external/md5/*.h")
add_library(md5 EXCLUDE_FROM_ALL OBJECT ${DEP_MD5_SRC})
set(DEP_MD5 $<TARGET_OBJECTS:md5>)

if(WEBSOCKETS)
  file(GLOB DEP_WEBSOCKETS_SRC "src/engine/external/libwebsockets/*.c" "src/engine/external/libwebsockets/*.h")
  add_library(websockets EXCLUDE_FROM_ALL OBJECT ${DEP_WEBSOCKETS_SRC})
  set(DEP_WEBSOCKETS $<TARGET_OBJECTS:websockets>)
else()
  set(DEP_WEBSOCKETS)
endif()

set(DEPS ${DEP_MD5} ${DEP_WEBSOCKETS} ${DEP_ZLIB})

# Libraries
set(LIBS ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARIES} ${PLATFORM_LIBS})

# Targets
add_library(engine-shared EXCLUDE_FROM_ALL OBJECT ${ENGINE_SHARED} ${BASE})
add_library(game-shared EXCLUDE_FROM_ALL OBJECT ${GAME_SHARED} ${GAME_GENERATED_SHARED})
list(APPEND TARGETS_OWN engine-shared game-shared)


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

  set(DEPS_CLIENT ${DEPS} ${DEP_JSON} ${DEP_PNG} ${DEP_WAV})

  # Libraries
  set(LIBS_CLIENT
    ${LIBS}
    ${CURL_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${OGG_LIBRARIES}
    ${OPUSFILE_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${PLATFORM_CLIENT_LIBS}
  )

  # Target
  set(TARGET_CLIENT ${CLIENT_EXECUTABLE})
  add_executable(${TARGET_CLIENT}
    ${CLIENT_SRC}
    ${DEPS_CLIENT}
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
    ${DEP_ZLIB}
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
  list(APPEND TARGETS_OWN ${TARGET_CLIENT})

  if(TARGET_OS STREQUAL "windows")
    target_sources(${TARGET_CLIENT} PRIVATE "other/icons/DDNet.rc")
  endif()
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
  ${DEPS}
  ${SERVER_SRC}
  $<TARGET_OBJECTS:engine-shared>
  $<TARGET_OBJECTS:game-shared>
)
target_link_libraries(${TARGET_SERVER} ${LIBS_SERVER})
list(APPEND TARGETS_OWN ${TARGET_SERVER})

if(MYSQL)
  target_compile_definitions(${TARGET_SERVER} PRIVATE -DCONF_SQL)
  target_include_directories(${TARGET_SERVER} PRIVATE ${MYSQL_INCLUDE_DIRS})
endif()

if(TARGET_OS STREQUAL "windows")
  target_sources(${TARGET_SERVER} PRIVATE "other/icons/DDNet-Server.rc")
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

add_executable(${TARGET_MASTERSRV} EXCLUDE_FROM_ALL ${MASTERSRV_SRC} $<TARGET_OBJECTS:engine-shared> ${DEPS})
add_executable(${TARGET_VERSIONSRV} EXCLUDE_FROM_ALL ${VERSIONSRV_SRC} $<TARGET_OBJECTS:engine-shared> ${DEPS})
add_executable(${TARGET_TWPING} EXCLUDE_FROM_ALL ${TWPING_SRC} $<TARGET_OBJECTS:engine-shared> ${DEPS})

target_link_libraries(${TARGET_MASTERSRV} ${LIBS})
target_link_libraries(${TARGET_VERSIONSRV} ${LIBS})
target_link_libraries(${TARGET_TWPING} ${LIBS})

list(APPEND TARGETS_OWN ${TARGET_MASTERSRV} ${TARGET_TWPING} ${TARGET_VERSIONSRV})

set(TARGETS_TOOLS)
file(GLOB TOOLS RELATIVE "${PROJECT_SOURCE_DIR}/src/tools/" "src/tools/*.cpp")
foreach(T ${TOOLS})
  string(REGEX REPLACE "\\.cpp$" "" TOOL "${T}")
  add_executable(${TOOL} EXCLUDE_FROM_ALL
    ${DEPS}
    src/tools/${TOOL}.cpp
    $<TARGET_OBJECTS:engine-shared>
  )
  if(TOOL MATCHES "^(tileset_|dilate$)")
    target_sources(${TOOL} PRIVATE ${DEP_PNG})
  endif()
  if(TOOL MATCHES "^config_")
    target_sources(${TOOL} PRIVATE "src/tools/config_common.h")
  endif()
  target_link_libraries(${TOOL} ${LIBS})
  list(APPEND TARGETS_TOOLS ${TOOL})
endforeach()

list(APPEND TARGETS_OWN ${TARGETS_TOOLS})

add_custom_target(tools DEPENDS ${TARGETS_TOOLS})
add_custom_target(everything DEPENDS ${TARGETS_OWN})

########################################################################
# COMPILER-SPECIFICS
########################################################################

# In the future (CMake 3.8.0+), use source_group(TREE ...)
macro(source_group_tree dir)
  file(GLOB ents RELATIVE ${PROJECT_SOURCE_DIR}/${dir} ${PROJECT_SOURCE_DIR}/${dir}/*)
  foreach(ent ${ents})
    if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${dir}/${ent})
      source_group_tree(${dir}/${ent})
    else()
      string(REPLACE "/" "\\" group ${dir})
      source_group(${group} FILES ${PROJECT_SOURCE_DIR}/${dir}/${ent})
    endif()
  endforeach()
endmacro()
source_group_tree(src)

set(TARGETS ${TARGETS_OWN} ${TARGETS_DEP})

foreach(target ${TARGETS})
  if(MSVC)
    target_compile_options(${target} PRIVATE /EHsc) # Only catch C++ exceptions with catch.
    target_compile_options(${target} PRIVATE /GS) # Protect the stack pointer.
    target_compile_options(${target} PRIVATE /wd4996) # Use of non-_s functions.
  elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(${target} PRIVATE -fstack-protector-all) # Protect the stack pointer.
    target_compile_definitions(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=1>) # Detect some buffer overflows.
  endif()
endforeach()

foreach(target ${TARGETS_OWN})
  if(MSVC)
    target_compile_options(${target} PRIVATE /wd4244) # Possible loss of data (float -> int, int -> float, etc.).
    target_compile_options(${target} PRIVATE /wd4800) # Implicit conversion of int to bool.
  elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(${target} PRIVATE -Wall)
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wdeclaration-after-statement>)
    #target_compile_options(${target} PRIVATE -Wextra)
    #target_compile_options(${target} PRIVATE -Wformat=2) # Warn about format strings.
  endif()
  target_include_directories(${target} PRIVATE src)
  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:CONF_DEBUG>)
  target_include_directories(${target} PRIVATE ${ZLIB_INCLUDEDIR})
  if(WEBSOCKETS)
    target_compile_definitions(${target} PRIVATE CONF_WEBSOCKETS)
  endif()
endforeach()

foreach(target ${TARGET_DEP})
  if(MSVC)
    target_compile_options(${target} PRIVATE /W0)
  endif()
endforeach()
