cmake_minimum_required(VERSION 2.8)

file(STRINGS src/game/version.h VERSION_LINE
  LIMIT_COUNT 1
  REGEX GAME_RELEASE_VERSION
)

if(VERSION_LINE MATCHES "\"([0-9]+)\\.([0-9]+)\\.([0-9]+)\"")
  set(VERSION_MAJOR ${CMAKE_MATCH_1})
  set(VERSION_MINOR ${CMAKE_MATCH_2})
  set(VERSION_PATCH ${CMAKE_MATCH_3})
else()
  message(FATAL_ERROR "Couldn't parse version from src/game/version.h")
endif()

if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
  project(DDNet VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
else()
  project(DDNet)
  set(PROJECT_VERSION_MAJOR ${VERSION_MAJOR})
  set(PROJECT_VERSION_MINOR ${VERSION_MINOR})
  set(PROJECT_VERSION_PATCH ${VERSION_PATCH})
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TARGET_BITS "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(TARGET_BITS "32")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(TARGET_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(TARGET_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(TARGET_OS "mac")
endif()

set(PREFER_BUNDLED_LIBS_DEFAULT OFF)
if(TARGET_OS STREQUAL "windows")
  set(PREFER_BUNDLED_LIBS_DEFAULT ON)
endif()

option(WEBSOCKETS "Enable websockets support" OFF)
option(MYSQL "Enable mysql support" OFF)
option(CLIENT "Compile client" ON)
option(PREFER_BUNDLED_LIBS "Prefer bundled libraries over system libraries" ${PREFER_BUNDLED_LIBS_DEFAULT})

# Set the default build type to Release
if(NOT(CMAKE_BUILD_TYPE))
  set(CMAKE_BUILD_TYPE Release)
endif()

set(SERVER_EXECUTABLE DDNet-Server CACHE STRING "Name of the built server executable")
set(CLIENT_EXECUTABLE DDNet CACHE STRING "Name of the build client executable")

########################################################################
# DEPENDENCIES
########################################################################

function(set_extra_dirs VARIABLE NAME)
  set("PATHS_${VARIABLE}_LIBDIR" PARENT_SCOPE)
  set("HINTS_${VARIABLE}_LIBDIR" PARENT_SCOPE)
  set("PATHS_${VARIABLE}_INCLUDEDIR" PARENT_SCOPE)
  set("HINTS_${VARIABLE}_INCLUDEDIR" PARENT_SCOPE)
  if(PREFER_BUNDLED_LIBS)
    set(TYPE HINTS)
  else()
    set(TYPE PATHS)
  endif()
  if(TARGET_BITS AND TARGET_OS)
    set(DIR "ddnet-libs/${NAME}/${TARGET_OS}/lib${TARGET_BITS}")
    set("${TYPE}_${VARIABLE}_LIBDIR" "${DIR}" PARENT_SCOPE)
    set("EXTRA_${VARIABLE}_LIBDIR" "${DIR}" PARENT_SCOPE)
  endif()
  set("${TYPE}_${VARIABLE}_INCLUDEDIR" "ddnet-libs/${NAME}/include" PARENT_SCOPE)
endfunction()

# Check for PkgConfig once so all the other `find_package` calls can do it
# quietly.
find_package(PkgConfig)
find_package(Curl)
find_package(Freetype)
find_package(GTest)
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
if(NOT PREFER_BUNDLED_LIBS)
  find_package(ZLIB)
else()
  set(ZLIB_FOUND NO)
endif()

message(STATUS "******** DDNet ********")
message(STATUS "Target OS: ${TARGET_OS} ${TARGET_BITS}bit")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

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
show_dependency_status("GTest" ${GTEST_FOUND} "${GTEST_LIBRARY}")
if(MYSQL)
  show_dependency_status("MySQL" ${MYSQL_FOUND} "${MYSQL_LIBRARY}")
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

if(NOT(PYTHONINTERP_FOUND))
  message(SEND_ERROR "You must install Python to compile DDNet")
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
if(NOT(GTEST_FOUND))
  message("To run the tests, you have to install GTest")
endif()

if(TARGET_OS STREQUAL "windows")
  set(PLATFORM_CLIENT)
  set(PLATFORM_CLIENT_LIBS opengl32 glu32 winmm)
  set(PLATFORM_LIBS ws2_32) # Windows sockets
elseif(TARGET_OS STREQUAL "mac")
  find_library(CARBON Carbon)
  find_library(COCOA Cocoa)
  find_library(OPENGL OpenGL)
  set(PLATFORM_CLIENT
    src/osx/notification.h
    src/osx/notification.mm
    src/osxlaunch/client.h
    src/osxlaunch/client.m
  )
  set(PLATFORM_CLIENT_LIBS ${COCOA} ${OPENGL})
  set(PLATFORM_LIBS ${CARBON})
else()
  set(PLATFORM_CLIENT)
  set(PLATFORM_CLIENT_LIBS GL GLU X11)
  if(TARGET_OS STREQUAL "linux")
    set(PLATFORM_LIBS rt) # clock_gettime for glibc < 2.17
  else()
    set(PLATFORM_LIBS)
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
  include(CheckCCompilerFlag)
  check_c_compiler_flag("-O2;-Wp,-Werror;-D_FORTIFY_SOURCE=2" DEFINE_FORTIFY_SOURCE) # Some distributions define _FORTIFY_SOURCE by themselves.
  check_c_compiler_flag("-fstack-protector-all" ENABLE_STACK_PROTECTOR) # -fstack-protector-all doesn't work on MinGW.
endif()


########################################################################
# INITALIZE TARGET LISTS
########################################################################

set(TARGETS_OWN)
set(TARGETS_DEP)

set(TARGETS_LINK) # Targets with a linking stage.

########################################################################
# COMMON FUNCTIONS
########################################################################

function(set_glob VAR GLOBBING DIRECTORY) # ...
  file(${GLOBBING} GLOB_RESULT "${DIRECTORY}/*.c" "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.h")
  list(SORT GLOB_RESULT)
  set(FILES)
  foreach(file ${ARGN})
    list(APPEND FILES "${PROJECT_SOURCE_DIR}/${DIRECTORY}/${file}")
  endforeach()

  if(NOT FILES STREQUAL GLOB_RESULT)
    message(AUTHOR_WARNING "${VAR} does not contain every file from directory ${DIRECTORY}")
    set(LIST_BUT_NOT_GLOB)
    if(POLICY CMP0057)
      cmake_policy(SET CMP0057 NEW)
      foreach(file ${FILES})
        if(NOT file IN_LIST GLOB_RESULT)
          list(APPEND LIST_BUT_NOT_GLOB ${file})
        endif()
      endforeach()
      if(LIST_BUT_NOT_GLOB)
        message(AUTHOR_WARNING "Entries only present in ${VAR}: ${LIST_BUT_NOT_GLOB}")
      endif()
      set(GLOB_BUT_NOT_LIST)
      foreach(file ${GLOB_RESULT})
        if(NOT file IN_LIST FILES)
          list(APPEND GLOB_BUT_NOT_LIST ${file})
        endif()
      endforeach()
      if(GLOB_BUT_NOT_LIST)
        message(AUTHOR_WARNING "Entries only present in ${DIRECTORY}: ${GLOB_BUT_NOT_LIST}")
      endif()
    endif()
  endif()

  set(${VAR} ${FILES} PARENT_SCOPE)
endfunction()

########################################################################
# DEPENDENCY COMPILATION
########################################################################

if(NOT(ZLIB_FOUND))
  set(ZLIB_LIBRARIES)
  set_glob(ZLIB_SRC GLOB src/engine/external/zlib
    adler32.c
    compress.c
    crc32.c
    crc32.h
    deflate.c
    deflate.h
    gzclose.c
    gzguts.h
    gzlib.c
    gzread.c
    gzwrite.c
    infback.c
    inffast.c
    inffast.h
    inffixed.h
    inflate.c
    inflate.h
    inftrees.c
    inftrees.h
    trees.c
    trees.h
    uncompr.c
    zconf.h
    zlib.h
    zutil.c
    zutil.h
  )
  add_library(zlib EXCLUDE_FROM_ALL OBJECT ${ZLIB_SRC})

  list(APPEND TARGETS_DEP zlib)
  set(ZLIB_INCLUDEDIR src/engine/external/zlib/)
  set(DEP_ZLIB $<TARGET_OBJECTS:zlib>)
else()
  set(ZLIB_INCLUDEDIR)
  set(DEP_ZLIB)
endif()

set_glob(DEP_PNG_SRC GLOB src/engine/external/pnglite pnglite.c pnglite.h)
add_library(png OBJECT EXCLUDE_FROM_ALL ${DEP_PNG_SRC})
target_include_directories(png PRIVATE ${ZLIB_INCLUDEDIR})

set(DEP_PNG $<TARGET_OBJECTS:png>)
list(APPEND TARGETS_DEP png)

if(CLIENT)
  # Static dependencies
  set_glob(DEP_JSON_SRC GLOB src/engine/external/json-parser json.c json.h)
  set_glob(DEP_WAV_SRC GLOB src/engine/external/wavpack
    bits.c
    float.c
    metadata.c
    unpack.c
    wavpack.h
    words.c
    wputils.c
  )
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
set(COPY_FILES
  ${CURL_COPY_FILES}
  ${FREETYPE_COPY_FILES}
  ${OPUSFILE_COPY_FILES}
  ${SDL2_COPY_FILES}
)
file(COPY ${COPY_FILES} DESTINATION .)

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

file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/src/game/generated/")
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
set_glob(BASE GLOB_RECURSE src/base
  color.h
  confusables.c
  confusables_data.h
  detect.h
  math.h
  system.c
  system.h
  tl/algorithm.h
  tl/allocator.h
  tl/array.h
  tl/base.h
  tl/range.h
  tl/sorted_array.h
  tl/string.h
  tl/threading.h
  vmath.h
)
set_glob(ENGINE_SHARED GLOB src/engine/shared
  compression.cpp
  compression.h
  config.cpp
  config.h
  config_variables.h
  console.cpp
  console.h
  datafile.cpp
  datafile.h
  demo.cpp
  demo.h
  econ.cpp
  econ.h
  engine.cpp
  fifo.cpp
  fifo.h
  filecollection.cpp
  filecollection.h
  global_uuid_manager.cpp
  huffman.cpp
  huffman.h
  jobs.cpp
  jobs.h
  kernel.cpp
  linereader.cpp
  linereader.h
  map.cpp
  masterserver.cpp
  memheap.cpp
  memheap.h
  message.h
  netban.cpp
  netban.h
  network.cpp
  network.h
  network_client.cpp
  network_conn.cpp
  network_console.cpp
  network_console_conn.cpp
  network_server.cpp
  packer.cpp
  packer.h
  protocol.h
  protocol_ex.cpp
  protocol_ex.h
  protocol_ex_msgs.h
  ringbuffer.cpp
  ringbuffer.h
  serverbrowser.cpp
  snapshot.cpp
  snapshot.h
  storage.cpp
  storage.h
  uuid_manager.cpp
  uuid_manager.h
  websockets.cpp
  websockets.h
)
set(ENGINE_GENERATED_SHARED src/game/generated/protocol.cpp src/game/generated/protocol.h)
set_glob(GAME_SHARED GLOB src/game
  collision.cpp
  collision.h
  ddracecommands.h
  extrainfo.cpp
  extrainfo.h
  gamecore.cpp
  gamecore.h
  layers.cpp
  layers.h
  localization.cpp
  localization.h
  mapitems.cpp
  mapitems.h
  teamscore.cpp
  teamscore.h
  tuning.h
  variables.h
  version.h
  voting.h
)
set(GAME_GENERATED_SHARED src/game/generated/nethash.cpp)

# Static dependencies
set_glob(DEP_MD5_SRC GLOB src/engine/external/md5 md5.c md5.h)
add_library(md5 EXCLUDE_FROM_ALL OBJECT ${DEP_MD5_SRC})
set(DEP_MD5 $<TARGET_OBJECTS:md5>)
list(APPEND TARGETS_DEP md5)

if(WEBSOCKETS)
  set_glob(DEP_WEBSOCKETS_SRC GLOB src/engine/external/libwebsockets
    alloc.c
    base64-decode.c
    config.h
    context.c
    extension-deflate-frame.c
    extension-deflate-frame.h
    extension-deflate-stream.c
    extension-deflate-stream.h
    extension.c
    getifaddrs.h
    handshake.c
    huftable.h
    lextable-strings.h
    lextable.h
    libwebsockets.c
    libwebsockets.h
    lws-plat-unix.c
    output.c
    parsers.c
    pollfd.c
    private-libwebsockets.h
    server-handshake.c
    server.c
    service.c
    sha-1.c
  )
  add_library(websockets EXCLUDE_FROM_ALL OBJECT ${DEP_WEBSOCKETS_SRC})
  list(APPEND TARGETS_DEP websockets)
  set(DEP_WEBSOCKETS $<TARGET_OBJECTS:websockets>)
else()
  set(DEP_WEBSOCKETS)
endif()

set(DEPS ${DEP_MD5} ${DEP_WEBSOCKETS} ${DEP_ZLIB})

# Libraries
set(LIBS ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARIES} ${PLATFORM_LIBS})

# Targets
add_library(engine-shared EXCLUDE_FROM_ALL OBJECT ${ENGINE_SHARED} ${ENGINE_GENERATED_SHARED} ${BASE})
add_library(game-shared EXCLUDE_FROM_ALL OBJECT ${GAME_SHARED} ${GAME_GENERATED_SHARED})
list(APPEND TARGETS_OWN engine-shared game-shared)


########################################################################
# CLIENT
########################################################################

if(CLIENT)
  # Sources
  set_glob(ENGINE_CLIENT GLOB src/engine/client
    backend_sdl.cpp
    backend_sdl.h
    client.cpp
    client.h
    fetcher.cpp
    fetcher.h
    friends.cpp
    friends.h
    graphics_threaded.cpp
    graphics_threaded.h
    input.cpp
    input.h
    keynames.h
    serverbrowser.cpp
    serverbrowser.h
    sound.cpp
    sound.h
    text.cpp
    updater.cpp
    updater.h
  )
  set_glob(GAME_CLIENT GLOB_RECURSE src/game/client
    animstate.cpp
    animstate.h
    component.h
    components/background.cpp
    components/background.h
    components/binds.cpp
    components/binds.h
    components/broadcast.cpp
    components/broadcast.h
    components/camera.cpp
    components/camera.h
    components/chat.cpp
    components/chat.h
    components/console.cpp
    components/console.h
    components/controls.cpp
    components/controls.h
    components/countryflags.cpp
    components/countryflags.h
    components/damageind.cpp
    components/damageind.h
    components/debughud.cpp
    components/debughud.h
    components/effects.cpp
    components/effects.h
    components/emoticon.cpp
    components/emoticon.h
    components/flow.cpp
    components/flow.h
    components/ghost.cpp
    components/ghost.h
    components/hud.cpp
    components/hud.h
    components/items.cpp
    components/items.h
    components/killmessages.cpp
    components/killmessages.h
    components/mapimages.cpp
    components/mapimages.h
    components/maplayers.cpp
    components/maplayers.h
    components/mapsounds.cpp
    components/mapsounds.h
    components/menus.cpp
    components/menus.h
    components/menus_browser.cpp
    components/menus_demo.cpp
    components/menus_ingame.cpp
    components/menus_settings.cpp
    components/motd.cpp
    components/motd.h
    components/nameplates.cpp
    components/nameplates.h
    components/particles.cpp
    components/particles.h
    components/players.cpp
    components/players.h
    components/race.cpp
    components/race.h
    components/race_demo.cpp
    components/race_demo.h
    components/scoreboard.cpp
    components/scoreboard.h
    components/skins.cpp
    components/skins.h
    components/sounds.cpp
    components/sounds.h
    components/spectator.cpp
    components/spectator.h
    components/statboard.cpp
    components/statboard.h
    components/voting.cpp
    components/voting.h
    gameclient.cpp
    gameclient.h
    lineinput.cpp
    lineinput.h
    render.cpp
    render.h
    render_map.cpp
    ui.cpp
    ui.h
  )
  set_glob(GAME_EDITOR GLOB src/game/editor
    auto_map.cpp
    auto_map.h
    editor.cpp
    editor.h
    io.cpp
    layer_game.cpp
    layer_quads.cpp
    layer_sounds.cpp
    layer_tiles.cpp
    popups.cpp
  )
  set(GAME_GENERATED_CLIENT
    src/game/generated/client_data.cpp
    src/game/generated/client_data.h
  )
  set(CLIENT_SRC ${ENGINE_CLIENT} ${PLATFORM_CLIENT} ${GAME_CLIENT} ${GAME_EDITOR} ${GAME_GENERATED_CLIENT})

  set(DEPS_CLIENT ${DEPS} ${DEP_JSON} ${DEP_PNG} ${DEP_WAV})

  # Libraries
  set(LIBS_CLIENT
    ${LIBS}
    ${CURL_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${SDL2_LIBRARIES}

    # Order of these three is important.
    ${OPUSFILE_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${OGG_LIBRARIES}

    ${PLATFORM_CLIENT_LIBS}
  )

  if(TARGET_OS STREQUAL "windows")
    if(MSVC)
      set(CLIENT_ICON "other/icons/DDNet_cl.rc")
    else()
      set(CLIENT_ICON "other/icons/DDNet_gcc.rc")
    endif()
  else()
    set(CLIENT_ICON)
  endif()

  # Target
  set(TARGET_CLIENT ${CLIENT_EXECUTABLE})
  add_executable(${TARGET_CLIENT}
    ${CLIENT_SRC}
    ${CLIENT_ICON}
    ${DEPS_CLIENT}
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
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
  list(APPEND TARGETS_LINK ${TARGET_CLIENT})
endif()


########################################################################
# SERVER
########################################################################

# Sources
set_glob(ENGINE_SERVER GLOB src/engine/server
  authmanager.cpp
  authmanager.h
  register.cpp
  register.h
  server.cpp
  server.h
  sql_connector.cpp
  sql_connector.h
  sql_server.cpp
  sql_server.h
  sql_string_helpers.cpp
  sql_string_helpers.h
)
set_glob(GAME_SERVER GLOB_RECURSE src/game/server
  ddracechat.cpp
  ddracechat.h
  ddracecommands.cpp
  entities/character.cpp
  entities/character.h
  entities/door.cpp
  entities/door.h
  entities/dragger.cpp
  entities/dragger.h
  entities/flag.cpp
  entities/flag.h
  entities/gun.cpp
  entities/gun.h
  entities/laser.cpp
  entities/laser.h
  entities/light.cpp
  entities/light.h
  entities/pickup.cpp
  entities/pickup.h
  entities/plasma.cpp
  entities/plasma.h
  entities/projectile.cpp
  entities/projectile.h
  entity.cpp
  entity.h
  eventhandler.cpp
  eventhandler.h
  gamecontext.cpp
  gamecontext.h
  gamecontroller.cpp
  gamecontroller.h
  gamemodes/DDRace.cpp
  gamemodes/DDRace.h
  gamemodes/gamemode.h
  gameworld.cpp
  gameworld.h
  player.cpp
  player.h
  save.cpp
  save.h
  score.h
  score/file_score.cpp
  score/file_score.h
  score/sql_score.cpp
  score/sql_score.h
  teams.cpp
  teams.h
)
set(GAME_GENERATED_SERVER
  "src/game/generated/server_data.cpp"
  "src/game/generated/server_data.h"
)
set(SERVER_SRC ${ENGINE_SERVER} ${GAME_SERVER} ${GAME_GENERATED_SERVER})
if(TARGET_OS STREQUAL "windows")
  if(MSVC)
    set(CLIENT_ICON "other/icons/DDNet-Server_cl.rc")
  else()
    set(CLIENT_ICON "other/icons/DDNet-Server_gcc.rc")
  endif()
else()
  set(SERVER_ICON)
endif()

# Libraries
set(LIBS_SERVER ${LIBS} ${MYSQL_LIBRARIES})

# Target
set(TARGET_SERVER ${SERVER_EXECUTABLE})
add_executable(${TARGET_SERVER}
  ${DEPS}
  ${SERVER_SRC}
  ${SERVER_ICON}
  $<TARGET_OBJECTS:engine-shared>
  $<TARGET_OBJECTS:game-shared>
)
target_link_libraries(${TARGET_SERVER} ${LIBS_SERVER})
list(APPEND TARGETS_OWN ${TARGET_SERVER})
list(APPEND TARGETS_LINK ${TARGET_SERVER})

########################################################################
# VARIOUS TARGETS
########################################################################

set_glob(MASTERSRV_SRC GLOB src/mastersrv mastersrv.cpp mastersrv.h)
set_glob(TWPING_SRC GLOB src/twping twping.cpp)

set(TARGET_MASTERSRV mastersrv)
set(TARGET_TWPING twping)

add_executable(${TARGET_MASTERSRV} EXCLUDE_FROM_ALL ${MASTERSRV_SRC} $<TARGET_OBJECTS:engine-shared> ${DEPS})
add_executable(${TARGET_TWPING} EXCLUDE_FROM_ALL ${TWPING_SRC} $<TARGET_OBJECTS:engine-shared> ${DEPS})

add_custom_target(generate_nethash DEPENDS src/game/generated/nethash.cpp)

target_link_libraries(${TARGET_MASTERSRV} ${LIBS})
target_link_libraries(${TARGET_TWPING} ${LIBS})

list(APPEND TARGETS_OWN ${TARGET_MASTERSRV} ${TARGET_TWPING})
list(APPEND TARGETS_LINK ${TARGET_MASTERSRV} ${TARGET_TWPING})

set(TARGETS_TOOLS)
set_glob(TOOLS GLOB src/tools
  config_common.h
  config_retrieve.cpp
  config_store.cpp
  confusables.cpp
  crapnet.cpp
  dilate.cpp
  dummy_map.cpp
  fake_server.cpp
  map_diff.cpp
  map_extract.cpp
  map_replace_image.cpp
  map_resave.cpp
  map_version.cpp
  packetgen.cpp
  tileset_borderadd.cpp
  tileset_borderfix.cpp
  tileset_borderrem.cpp
  tileset_borderset.cpp
  uuid.cpp
)
foreach(ABS_T ${TOOLS})
  file(RELATIVE_PATH T "${PROJECT_SOURCE_DIR}/src/tools/" ${ABS_T})
  if(T MATCHES "\\.cpp$")
    string(REGEX REPLACE "\\.cpp$" "" TOOL "${T}")
    set(EXTRA_TOOL_SRC)
    set(EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
    if(TOOL MATCHES "^(tileset_|dilate|map_extract|map_replace_image$)")
      list(APPEND EXTRA_TOOL_SRC ${DEP_PNG})
    endif()
    if(TOOL MATCHES "^config_")
      list(APPEND EXTRA_TOOL_SRC "src/tools/config_common.h")
    endif()
    if(TOOL MATCHES "^(config_retrieve|config_store|dilate|map_diff|map_extract)$")
      set(EXCLUDE_FROM_ALL)
    endif()
    add_executable(${TOOL} ${EXCLUDE_FROM_ALL}
      ${DEPS}
      src/tools/${TOOL}.cpp
      ${EXTRA_TOOL_SRC}
      $<TARGET_OBJECTS:engine-shared>
    )
    target_link_libraries(${TOOL} ${LIBS})
    list(APPEND TARGETS_TOOLS ${TOOL})
  endif()
endforeach()

list(APPEND TARGETS_OWN ${TARGETS_TOOLS})
list(APPEND TARGETS_LINK ${TARGETS_TOOLS})

add_custom_target(tools DEPENDS ${TARGETS_TOOLS})
add_custom_target(everything DEPENDS ${TARGETS_OWN})

########################################################################
# TESTS
########################################################################

if(GTEST_FOUND)
  set_glob(TESTS GLOB src/test
    strip_path_and_extension.cpp
  )

  set(TARGET_TESTRUNNER testrunner)
  add_executable(${TARGET_TESTRUNNER} EXCLUDE_FROM_ALL
    ${TESTS}
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
    ${DEP_MD5}
  )
  target_link_libraries(${TARGET_TESTRUNNER} ${LIBS} ${GTEST_BOTH_LIBRARIES})
  target_include_directories(${TARGET_TESTRUNNER} PRIVATE ${GTEST_INCLUDE_DIRS})

  list(APPEND TARGETS_OWN ${TARGET_TESTRUNNER})
  list(APPEND TARGETS_LINK ${TARGET_TESTRUNNER})

  add_custom_target(run_tests
    COMMAND $<TARGET_FILE:${TARGET_TESTRUNNER}>
    COMMENT Running tests
    DEPENDS ${TARGET_TESTRUNNER}
    USES_TERMINAL
  )
endif()

########################################################################
# INSTALLATION
########################################################################

function(escape_regex VAR STRING)
  string(REGEX REPLACE "([][^$.+*?|()\\\\])" "\\\\\\1" ESCAPED "${STRING}")
  set(${VAR} ${ESCAPED} PARENT_SCOPE)
endfunction()

function(max_length VAR)
  set(MAX_LENGTH 0)
  foreach(str ${ARGN})
    string(LENGTH ${str} LENGTH)
    if(LENGTH GREATER MAX_LENGTH)
      set(MAX_LENGTH ${LENGTH})
    endif()
  endforeach()
  set(${VAR} ${MAX_LENGTH} PARENT_SCOPE)
endfunction()

# Tries to generate a list of regex that matches everything except the given
# parameters.
function(regex_inverted VAR)
  max_length(MAX_LENGTH ${ARGN})
  math(EXPR UPPER_BOUND "${MAX_LENGTH}-1")

  set(REMAINING ${ARGN})
  set(RESULT)

  foreach(i RANGE ${UPPER_BOUND})
    set(TEMP ${REMAINING})
    set(REMAINING)
    foreach(str ${TEMP})
      string(LENGTH ${str} LENGTH)
      if(i LESS LENGTH)
        list(APPEND REMAINING ${str})
      endif()
    endforeach()

    set(ADDITIONAL)
    foreach(outer ${REMAINING})
      string(SUBSTRING ${outer} 0 ${i} OUTER_PREFIX)
      set(CHARS "")
      foreach(inner ${REMAINING})
        string(SUBSTRING ${inner} 0 ${i} INNER_PREFIX)
        if(OUTER_PREFIX STREQUAL INNER_PREFIX)
          string(SUBSTRING ${inner} ${i} 1 INNER_NEXT)
          set(CHARS "${CHARS}${INNER_NEXT}")
        endif()
      endforeach()
      escape_regex(OUTER_PREFIX_ESCAPED "${OUTER_PREFIX}")

      list(APPEND ADDITIONAL "${OUTER_PREFIX_ESCAPED}([^${CHARS}]|$)")
    endforeach()
    list(REMOVE_DUPLICATES ADDITIONAL)
    list(APPEND RESULT ${ADDITIONAL})
  endforeach()
  set(${VAR} ${RESULT} PARENT_SCOPE)
endfunction()

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_GENERATOR TGZ TXZ)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL portable)
set(CPACK_SOURCE_GENERATOR ZIP TGZ TBZ2 TXZ)
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})
set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})

if(TARGET_OS AND TARGET_BITS)
  if(TARGET_OS STREQUAL "windows")
    set(CPACK_SYSTEM_NAME "win${TARGET_BITS}")
    set(CPACK_GENERATOR ZIP)
  elseif(TARGET_OS STREQUAL "linux")
    # Assuming Intel here.
    if(TARGET_BITS EQUAL 32)
      set(CPACK_SYSTEM_NAME "linux_x86")
    elseif(TARGET_BITS EQUAL 64)
      set(CPACK_SYSTEM_NAME "linux_x86_64")
    endif()
  elseif(TARGET_OS STREQUAL "mac")
    set(CPACK_SYSTEM_NAME "osx")
  endif()
endif()

set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-src)
set(CPACK_SOURCE_FILES
  CMakeLists.txt
  README.md
  autoexec_server.cfg
  bam.lua
  cmake/
  configure.lua
  data/
  datasrc/
  ddnet-libs/
  license.txt
  other/
  scripts/
  src/
  storage.cfg
)
set(CPACK_SOURCE_IGNORE_FILES
  "\\.pyc$"
  "/\\.git"
  "/__pycache__/"
)

regex_inverted(CPACK_SOURCE_FILES_INVERTED ${CPACK_SOURCE_FILES})
escape_regex(PROJECT_SOURCE_DIR_ESCAPED ${PROJECT_SOURCE_DIR})

foreach(str ${CPACK_SOURCE_FILES_INVERTED})
  list(APPEND CPACK_SOURCE_IGNORE_FILES "${PROJECT_SOURCE_DIR_ESCAPED}/${str}")
endforeach()

# Unset these variables, they might do something in the future of CPack.
unset(CPACK_SOURCE_FILES)
unset(CPACK_SOURCE_FILES_INVERTED)

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ${PROJECT_NAME})
install(TARGETS ${TARGET_CLIENT} ${TARGET_SERVER} DESTINATION bin)
install(DIRECTORY data DESTINATION share/DDNet)

set(EXTRA_ARGS DESTINATION ${CPACK_PACKAGE_FILE_NAME} COMPONENT portable EXCLUDE_FROM_ALL)
install(TARGETS ${TARGET_CLIENT} ${TARGET_SERVER} ${EXTRA_ARGS})
install(TARGETS config_retrieve config_store dilate map_diff map_extract ${EXTRA_ARGS} OPTIONAL)
install(DIRECTORY data ${EXTRA_ARGS})
install(FILES license.txt storage.cfg autoexec_server.cfg ${EXTRA_ARGS})
install(FILES ${COPY_FILES} ${EXTRA_ARGS})
if(TARGET_OS STREQUAL "windows")
  install(FILES other/config_directory.bat ${EXTRA_ARGS})
endif()

include(CPack)

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
    set(DBG $<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>)
    target_compile_options(${target} PRIVATE $<$<NOT:${DBG}>:/MT> $<${DBG}:/MTd>) # Use static CRT
    target_compile_options(${target} PRIVATE /MP) # Use multiple cores
    target_compile_options(${target} PRIVATE /EHsc) # Only catch C++ exceptions with catch.
    target_compile_options(${target} PRIVATE /GS) # Protect the stack pointer.
    target_compile_options(${target} PRIVATE /wd4996) # Use of non-_s functions.
  elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
    if(ENABLE_STACK_PROTECTOR)
      target_compile_options(${target} PRIVATE -fstack-protector-all) # Protect the stack pointer.
    endif()
    if(DEFINE_FORTIFY_SOURCE)
      target_compile_definitions(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>) # Detect some buffer overflows.
    endif()
  endif()
  if(TARGET_OS STREQUAL "mac")
    target_compile_options(${target} PRIVATE -stdlib=libc++)
    target_compile_options(${target} PRIVATE -mmacosx-version-min=10.7)
  endif()
endforeach()

if(MSVC)
  set_property(TARGET ${TARGET_CLIENT} APPEND PROPERTY LINK_FLAGS /SAFESEH:NO) # Disable SafeSEH because the shipped libraries don't support it.
endif()

foreach(target ${TARGETS_LINK})
  if(TARGET_OS STREQUAL "mac")
    target_link_libraries(${target} -stdlib=libc++)
    target_link_libraries(${target} -mmacosx-version-min=10.7)
  endif()
  if(MINGW)
    # Statically link the standard libraries with on MinGW so we don't have to
    # ship them as DLLs.
    target_link_libraries(${target} -static-libgcc)
    target_link_libraries(${target} -static-libstdc++)
  endif()
endforeach()

foreach(target ${TARGETS_OWN})
  if(MSVC)
    target_compile_options(${target} PRIVATE /wd4244) # Possible loss of data (float -> int, int -> float, etc.).
    target_compile_options(${target} PRIVATE /wd4267) # Possible loss of data (size_t - int on win64).
    target_compile_options(${target} PRIVATE /wd4800) # Implicit conversion of int to bool.
  elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU)
    target_compile_options(${target} PRIVATE -Wall)
    if(CMAKE_VERSION VERSION_GREATER 3.3 OR CMAKE_VERSION VERSION_EQUAL 3.3)
      target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wdeclaration-after-statement>)
    endif()
    target_compile_options(${target} PRIVATE -Wextra)
    target_compile_options(${target} PRIVATE -Wno-unused-parameter)
    target_compile_options(${target} PRIVATE -Wno-missing-field-initializers)
    target_compile_options(${target} PRIVATE -Wformat=2) # Warn about format strings.
  endif()
  target_include_directories(${target} PRIVATE src)
  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:CONF_DEBUG>)
  target_include_directories(${target} PRIVATE ${ZLIB_INCLUDEDIR})
  if(WEBSOCKETS)
    target_compile_definitions(${target} PRIVATE CONF_WEBSOCKETS)
  endif()
  if(MYSQL)
    target_compile_definitions(${target} PRIVATE CONF_SQL)
    target_include_directories(${target} PRIVATE ${MYSQL_INCLUDE_DIRS})
  endif()
endforeach()

foreach(target ${TARGETS_DEP})
  if(MSVC)
    target_compile_options(${target} PRIVATE /W0)
  endif()
endforeach()
