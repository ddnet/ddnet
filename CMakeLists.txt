cmake_minimum_required(VERSION 3.12...3.27.4)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE INTERNAL "Minimum macOS deployment version")
if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 10.15)
  message(WARNING "Building for macOS < 10.15 is not supported")
endif()

file(STRINGS src/game/version.h VERSION_LINE
  LIMIT_COUNT 1
  REGEX "^#define GAME_RELEASE_VERSION "
)

if(VERSION_LINE MATCHES "\"([0-9]+)\\.([0-9]+)\\.([0-9]+)\"")
  set(VERSION_MAJOR ${CMAKE_MATCH_1})
  set(VERSION_MINOR ${CMAKE_MATCH_2})
  set(VERSION_PATCH ${CMAKE_MATCH_3})
elseif(VERSION_LINE MATCHES "\"([0-9]+)\\.([0-9]+)\"")
  set(VERSION_MAJOR ${CMAKE_MATCH_1})
  set(VERSION_MINOR ${CMAKE_MATCH_2})
  set(VERSION_PATCH "0")
else()
  message(FATAL_ERROR "Couldn't parse version from src/game/version.h")
endif()

if(VERSION_PATCH STREQUAL "0")
  project(DDNet VERSION ${VERSION_MAJOR}.${VERSION_MINOR})
else()
  project(DDNet VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
endif()

set(ORIGINAL_CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH})
set(ORIGINAL_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
set(ORIGINAL_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
set(OWN_CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(TARGET_BITS "32")
else()
  set(TARGET_BITS "64")
endif()

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm"
  OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64"
  OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "ARM64")
  if(TARGET_BITS STREQUAL "32")
    set(TARGET_CPU_ARCHITECTURE "arm")
  else()
    set(TARGET_CPU_ARCHITECTURE "arm64")
  endif()
else()
  if(TARGET_BITS STREQUAL "32")
    set(TARGET_CPU_ARCHITECTURE "x86")
  else()
    set(TARGET_CPU_ARCHITECTURE "x86_64")
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(TARGET_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(TARGET_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(TARGET_OS "mac")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
  set(TARGET_OS "haiku")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(TARGET_OS "android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(TARGET_OS "emscripten")
endif()

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.18)
  include(CheckLinkerFlag)
endif()
include(CheckSymbolExists)
if(NOT (TARGET_OS STREQUAL "emscripten"))
  include(CheckAtomic)
endif()

check_symbol_exists(__i386 "" TARGET_ARCH_X86_i386)
if(TARGET_ARCH_X86_i386)
  set(TARGET_ARCH x86)
else()
  set(TARGET_ARCH)
endif()

set(AUTO_DEPENDENCIES_DEFAULT OFF)

set(AUTO_VULKAN_BACKEND ON)
if(TARGET_OS STREQUAL "windows")
  set(AUTO_DEPENDENCIES_DEFAULT ON)
  if(TARGET_CPU_ARCHITECTURE STREQUAL "x86")
    set(AUTO_VULKAN_BACKEND OFF)
  endif()
elseif(TARGET_OS STREQUAL "mac")
  set(AUTO_VULKAN_BACKEND OFF)
endif()

option(WEBSOCKETS "Enable websockets support" OFF)
option(MYSQL "Enable mysql support" OFF)
option(TEST_MYSQL "Test mysql support in unit tests (also sets -DMYSQL=ON)" OFF)
option(AUTOUPDATE "Enable the autoupdater" OFF)
option(INFORM_UPDATE "Inform about available updates" ON)
option(VIDEORECORDER "Enable video recording support via FFmpeg" ON)
option(UPNP "Enable UPnP support" OFF)
option(ANTIBOT "Enable support for a dynamic anticheat library (not provided, see src/antibot for interface if you want to implement your own)" OFF)
option(HEADLESS_CLIENT "Build the client without graphics" OFF)
option(CLIENT "Compile client" ON)
option(SERVER "Compile server" ON)
option(TOOLS "Compile tools" ON)
option(DOWNLOAD_GTEST "Download and compile GTest" ${AUTO_DEPENDENCIES_DEFAULT})
option(STEAM "Build the Steam release version" OFF)
option(DISCORD "Enable Discord rich presence support" OFF)
option(DISCORD_DYNAMIC "Enable discovering Discord rich presence libraries at runtime (Linux only)" OFF)
option(PREFER_BUNDLED_LIBS "Prefer bundled libraries over system libraries" ${AUTO_DEPENDENCIES_DEFAULT})
option(DEV "Don't generate stuff necessary for packaging" OFF)
option(VULKAN "Enable the vulkan backend" ${AUTO_VULKAN_BACKEND})

option(EXCEPTION_HANDLING "Enable crashdumps (only works with Windows)" OFF)
option(IPO "Enable interprocedural optimizations" OFF)
option(FUSE_LD "Linker to use" OFF)
option(SECURITY_COMPILER_FLAGS "Whether to set security-relevant compiler flags like -D_FORTIFY_SOURCE=2 and -fstack-protector-strong" ON)

if(TARGET_OS STREQUAL "emscripten")
  include(${PROJECT_SOURCE_DIR}/cmake/toolchains/Emscripten.toolchain)
endif()

if(TEST_MYSQL)
  set(MYSQL ON)
endif()

# Set version if not explicitly set
if(NOT VERSION)
  set(VERSION ${PROJECT_VERSION})
endif()

set(OpenGL_GL_PREFERENCE LEGACY)

# Set the default build type to Release
if(NOT(CMAKE_BUILD_TYPE))
  if(NOT(DEV))
    set(CMAKE_BUILD_TYPE Release)
  else()
    set(CMAKE_BUILD_TYPE Debug)
  endif()
endif()

if(NOT(CMAKE_BUILD_TYPE MATCHES "^(Release|Debug|RelWithDebInfo|MinSizeRel)$"))
  message(WARNING "Unknown CMAKE_BUILD_TYPE, should be one of Release, Debug, RelWithDebInfo or MinSizeRel")
endif()

set(DBG $<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>)

if(IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
  if(ipo_supported)
    message(STATUS "IPO is enabled")
    set(ENABLE_IPO TRUE)
  else()
    message(WARNING "IPO is not supported: ${ipo_output}")
  endif()
endif()

if(NOT "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  # Remove version.h generated by previous build code
  file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/src/game/version.h)
endif()

set(SERVER_EXECUTABLE DDNet-Server CACHE STRING "Name of the built server executable")
set(CLIENT_EXECUTABLE DDNet CACHE STRING "Name of the build client executable")

########################################################################
# Compiler flags
########################################################################

function(add_c_compiler_flag_if_supported VARIABLE FLAG)
  if(ARGC GREATER 2)
    set(CHECKED_FLAG "${ARGV2}")
  else()
    set(CHECKED_FLAG "${FLAG}")
  endif()
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" CONFIG_VARIABLE "FLAG_SUPPORTED${CHECKED_FLAG}")
  check_c_compiler_flag("${CHECKED_FLAG}" ${CONFIG_VARIABLE})
  if(${CONFIG_VARIABLE})
    if(${VARIABLE})
      set("${VARIABLE}" "${${VARIABLE}};${FLAG}" PARENT_SCOPE)
    else()
      set("${VARIABLE}" "${FLAG}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(add_cxx_compiler_flag_if_supported VARIABLE FLAG)
  if(ARGC GREATER 2)
    set(CHECKED_FLAG "${ARGV2}")
  else()
    set(CHECKED_FLAG "${FLAG}")
  endif()
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" CONFIG_VARIABLE "FLAG_SUPPORTED${CHECKED_FLAG}")
  check_cxx_compiler_flag("${CHECKED_FLAG}" ${CONFIG_VARIABLE})
  if(${CONFIG_VARIABLE})
    if(${VARIABLE})
      set("${VARIABLE}" "${${VARIABLE}};${FLAG}" PARENT_SCOPE)
    else()
      set("${VARIABLE}" "${FLAG}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(add_linker_flag_if_supported VARIABLE FLAG)
  if(ARGC GREATER 2)
    set(CHECKED_FLAG "${ARGV2}")
  else()
    set(CHECKED_FLAG "${FLAG}")
  endif()
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" CONFIG_VARIABLE "FLAG_SUPPORTED${CHECKED_FLAG}")
  if(CMAKE_VERSION VERSION_LESS 3.18)
    set(${CONFIG_VARIABLE} OFF)
  else()
    check_linker_flag(C "${CHECKED_FLAG}" ${CONFIG_VARIABLE})
  endif()
  if(${CONFIG_VARIABLE})
    if(${VARIABLE})
      set("${VARIABLE}" "${${VARIABLE}};${FLAG}" PARENT_SCOPE)
    else()
      set("${VARIABLE}" "${FLAG}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

# Force compiler colors on when using ninja. Ninja filters the colors out when
# it's not printing to a terminal on its own.
if(CMAKE_GENERATOR STREQUAL "Ninja")
  add_cxx_compiler_flag_if_supported(OUR_FLAGS -fdiagnostics-color=always)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS -fcolor-diagnostics)
endif()

if(NOT MSVC AND NOT HAIKU)
  if(NOT TARGET_OS STREQUAL "mac")
    if(NOT FUSE_LD STREQUAL OFF)
      add_linker_flag_if_supported(OUR_FLAGS_LINK -fuse-ld=${FUSE_LD})
      if(FLAG_SUPPORTED_fuse_ld_${FUSE_LD})
        message(STATUS "Using ${FUSE_LD} linker")
      endif()
    else()
      if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT ENABLE_IPO)
        # GCC+LTO: pthread_create has failed: Resource temporarily unavailable
        add_linker_flag_if_supported(OUR_FLAGS_LINK -fuse-ld=mold)
        if(FLAG_SUPPORTED_fuse_ld_mold)
          message(STATUS "Using mold linker")
        else()
          # Does not support GCC+LTO
          add_linker_flag_if_supported(OUR_FLAGS_LINK -fuse-ld=lld)
          if(FLAG_SUPPORTED_fuse_ld_lld)
            message(STATUS "Using lld linker")
          else()
            add_linker_flag_if_supported(OUR_FLAGS_LINK -fuse-ld=gold)
            if(FLAG_SUPPORTED_fuse_ld_gold)
              message(STATUS "Using gold linker")
            endif()
          endif()
        endif()
      else()
        add_linker_flag_if_supported(OUR_FLAGS_LINK -fuse-ld=gold)
        if(FLAG_SUPPORTED_fuse_ld_gold)
          message(STATUS "Using gold linker")
        endif()
      endif()
    endif()
  endif()

  if(TARGET_OS STREQUAL "mac")
    add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -std=gnu++17)
  endif()

  if(SECURITY_COMPILER_FLAGS)
    # Protect the stack pointer.
    add_cxx_compiler_flag_if_supported(OUR_FLAGS -fstack-protector-strong)
    if(MINGW)
      add_linker_flag_if_supported(OUR_FLAGS_LINK -lssp)
    endif()
  endif()

  # Disable exceptions as DDNet does not use them.
  add_cxx_compiler_flag_if_supported(OUR_FLAGS -fno-exceptions)

  # Inaccurate floating point numbers cause problems on mingw-w64-gcc when
  # compiling for x86, might cause problems elsewhere. So don't store floats
  # in registers but keep them at higher accuracy.
  if(TARGET_ARCH STREQUAL "x86")
    add_cxx_compiler_flag_if_supported(OUR_FLAGS -ffloat-store)
  endif()

  # We assume that char is signed in various places in the code. In particular,
  # the Str.StrToInts test will fail when char is not signed and names containing
  # special characters will be displayed incorrectly on servers.
  add_cxx_compiler_flag_if_supported(OUR_FLAGS -fsigned-char)

  # Don't insert timestamps into PEs to keep the build reproducible.
  if(TARGET_OS STREQUAL "windows")
    add_cxx_compiler_flag_if_supported(OUR_FLAGS_LINK -Wl,--no-insert-timestamp)
  endif()

  if(TARGET_OS STREQUAL "mac")
    add_cxx_compiler_flag_if_supported(OUR_FLAGS -stdlib=libc++)
  endif()

  if(EXCEPTION_HANDLING)
    # use the frame pointer (frame pointer usage is disabled by default in
    # some architectures like x86_64 and for some optimization levels; and it
    # may be impossible to walk the call stack without it)
    add_cxx_compiler_flag_if_supported(OUR_FLAGS -fno-omit-frame-pointer)
  endif()

  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wall)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wextra)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wno-psabi) # parameter passing for argument of type ‘__gnu_cxx::__normal_iterator<CCommandProcessorFragment_Vulkan::SMemoryBlock<1>*, std::vector<CCommandProcessorFragment_Vulkan::SMemoryBlock<1>, std::allocator<CCommandProcessorFragment_Vulkan::SMemoryBlock<1> > > >’ changed in GCC 7.1
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wno-unused-parameter)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wno-missing-field-initializers)
  if(CMAKE_BUILD_TYPE STREQUAL DEBUG)
    add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wformat=2) # Warn about format strings.
  else()
    add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wno-format) # Don't warn about format strings in release mode since our `str_format` optimization is incompatible with it.
  endif()
  add_c_compiler_flag_if_supported(OUR_FLAGS_DEP -Wno-implicit-function-declaration)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wno-nullability-completeness) # Mac OS build on github
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wduplicated-cond)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wduplicated-branches)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wlogical-op)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wrestrict)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wshadow-all) # clang
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wshadow=global) # gcc
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wthread-safety)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wthread-safety-negative)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wsuggest-override)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wdynamic-class-memaccess) # clang
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wclass-memaccess) # gcc
  add_linker_flag_if_supported(OUR_FLAGS_LINK -Wno-alloc-size-larger-than) # save.cpp with LTO
  # add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wdouble-promotion) # Many occurrences
  # add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wnull-dereference) # Many occurrences
  # add_cxx_compiler_flag_if_supported(OUR_FLAGS_OWN -Wuseless-cast) # TODO: Enable for C++ code except gtest
endif()

if(NOT MSVC AND NOT HAIKU AND SECURITY_COMPILER_FLAGS)
  check_c_compiler_flag("-O2;-Wp,-Werror;-D_FORTIFY_SOURCE=2" DEFINE_FORTIFY_SOURCE) # Some distributions define _FORTIFY_SOURCE by themselves.
endif()

########################################################################
# COMMON FUNCTIONS
########################################################################

function(set_glob VAR GLOBBING EXTS DIRECTORY)
  set(GLOBS)
  foreach(ext ${EXTS})
    list(APPEND GLOBS "${DIRECTORY}/*.${ext}")
  endforeach()
  file(${GLOBBING} GLOB_RESULT ${GLOBS})
  list(SORT GLOB_RESULT)
  set(FILES)

  set(${VAR} ${GLOB_RESULT} PARENT_SCOPE)
endfunction()

function(set_src VAR GLOBBING DIRECTORY) # ...
  set_glob(${VAR} ${GLOBBING} "c;cpp;h" ${DIRECTORY})
  set(${VAR} ${${VAR}} PARENT_SCOPE)
  set(CHECKSUM_SRC ${CHECKSUM_SRC} ${${VAR}} PARENT_SCOPE)
endfunction()
set(CHECKSUM_SRC)

function(set_own_rpath TARGET)
  if(NOT TARGET_OS STREQUAL "windows" AND NOT TARGET_OS STREQUAL "mac")
    set_property(TARGET ${TARGET} PROPERTY BUILD_RPATH "$ORIGIN")
    set_property(TARGET ${TARGET} PROPERTY INSTALL_RPATH "$ORIGIN/../lib/ddnet")
  endif()
endfunction()

########################################################################
# INITIALIZE TARGET LISTS
########################################################################

set(TARGETS_OWN)
set(TARGETS_DEP)

set(TARGETS_LINK) # Targets with a linking stage.

########################################################################
# DEPENDENCIES
########################################################################

if((CMAKE_OSX_ARCHITECTURES STREQUAL "arm64;x86_64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64;arm64") AND TARGET_OS STREQUAL "mac")
  set(FAT ON)
else()
  set(FAT OFF)
endif()

if(FAT)
  set(LIB_DIR "${TARGET_OS}/libfat")
elseif(TARGET_OS STREQUAL "emscripten")
  set(LIB_DIR "webasm/libwasm")
elseif(TARGET_CPU_ARCHITECTURE STREQUAL "arm" OR TARGET_CPU_ARCHITECTURE STREQUAL "arm64")
  set(LIB_DIR "${TARGET_OS}/lib${TARGET_CPU_ARCHITECTURE}")
else()
  set(LIB_DIR "${TARGET_OS}/lib${TARGET_BITS}")
endif()

function(set_extra_dirs_lib VARIABLE NAME)
  set("PATHS_${VARIABLE}_LIBDIR" PARENT_SCOPE)
  set("HINTS_${VARIABLE}_LIBDIR" PARENT_SCOPE)
  if(PREFER_BUNDLED_LIBS)
    set(TYPE HINTS)
  else()
    set(TYPE PATHS)
  endif()
  if(TARGET_BITS AND TARGET_OS)
    set(DIR "ddnet-libs/${NAME}/${LIB_DIR}")
    set("${TYPE}_${VARIABLE}_LIBDIR" "${DIR}" PARENT_SCOPE)
    set("EXTRA_${VARIABLE}_LIBDIR" "${DIR}" PARENT_SCOPE)
  endif()
endfunction()

function(set_extra_dirs_include VARIABLE NAME LIBRARY)
  set("PATHS_${VARIABLE}_INCLUDEDIR" PARENT_SCOPE)
  set("HINTS_${VARIABLE}_INCLUDEDIR" PARENT_SCOPE)
  is_bundled(IS_BUNDLED "${LIBRARY}")
  if(IS_BUNDLED)
    set(TMP_TARGET_OS ${TARGET_OS})
    if(TARGET_OS STREQUAL "emscripten")
     set(TMP_TARGET_OS webasm)
    endif()
    set("HINTS_${VARIABLE}_INCLUDEDIR" "ddnet-libs/${NAME}/include" "ddnet-libs/${NAME}/include/${TMP_TARGET_OS}" PARENT_SCOPE)
  endif()
endfunction()

if(CMAKE_CROSSCOMPILING)
  if(TARGET_OS STREQUAL "android" OR TARGET_OS STREQUAL "emscripten")
    # be more aggressive with android toolchain
    set(CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH NO_CMAKE_SYSTEM_PATH NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  else()
    set(CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH NO_CMAKE_SYSTEM_PATH)
  endif()
else()
  set(CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH)
endif()

function(is_bundled VARIABLE PATH)
  if(PATH)
    string(FIND "${PATH}" "${PROJECT_SOURCE_DIR}" LOCAL_PATH_POS)
    if(LOCAL_PATH_POS EQUAL 0 AND TARGET_BITS AND TARGET_OS)
      set("${VARIABLE}" ON PARENT_SCOPE)
    else()
      set("${VARIABLE}" OFF PARENT_SCOPE)
    endif()
  else()
    set("${VARIABLE}" OFF PARENT_SCOPE)
  endif()
endfunction()

if(NOT CMAKE_CROSSCOMPILING)
  # Check for PkgConfig once so all the other `find_package` calls can do it
  # quietly.
  find_package(PkgConfig)
endif()
if(TARGET_OS STREQUAL "android")
  find_package(Android)
endif()
find_package(ZLIB)
find_package(Crypto)
find_package(Curl)
if(CLIENT AND VIDEORECORDER)
  find_package(FFMPEG)
endif()
find_package(Freetype)
if(DOWNLOAD_GTEST)
  find_package(Git)
endif()
if(NOT(TARGET_OS STREQUAL "android"))
  find_package(GLEW)
endif()
find_package(GTest)
if(UPNP)
  find_package(Miniupnpc)
endif()
if(MYSQL)
  find_package(MySQL)
else()
  set(MYSQL_LIBRARIES)
endif()
find_package(Ogg)
find_package(Opus)
find_package(Opusfile)
find_package(PNG)
find_package(Python3)
find_package(Rust)
find_package(SDL2)
find_package(SQLite3)
if(DISCORD)
  find_package(DiscordSdk)
endif()
if(UNIX)
  # Use -pthread instead of -lpthread to draw dependencies other than libpthread
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
endif()
find_package(Threads)
find_package(Wavpack)
if(WEBSOCKETS)
  find_package(Websockets)
else()
  set(WEBSOCKETS_LIBRARIES)
  set(WEBSOCKETS_INCLUDE_DIRS)
endif()

if(EXCEPTION_HANDLING)
  find_package(Crashdump)
endif()

if(SECURITY_COMPILER_FLAGS)
  find_package(SSP)
endif()

if(TARGET_OS AND TARGET_OS STREQUAL "mac")
  find_program(CMAKE_OTOOL otool)
  find_program(DMGBUILD dmgbuild)
endif()

set(VULKAN_SHADER_FILE_LIST "" CACHE STRING "Vulkan shader file list")
set(VULKAN_SHADER_FILE_SHA256 "" CACHE STRING "Vulkan shader file hash")
if(CLIENT AND VULKAN)
  find_package(Vulkan)
  include(cmake/BuildVulkanShaders.cmake)
else()
  set(VULKAN_LIBRARIES)
  set(VULKAN_INCLUDE_DIRS)
endif()

message(STATUS "******** ${CMAKE_PROJECT_NAME} ********")
set(TARGET "Target OS: ${TARGET_OS} ${CMAKE_SYSTEM_PROCESSOR}")
if(TARGET_OS STREQUAL "mac")
  set(TARGET "${TARGET} (SDK: ${CMAKE_OSX_SYSROOT}, architectures: ${CMAKE_OSX_ARCHITECTURES})")
endif()
message(STATUS ${TARGET})
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Rust version: ${RUST_VERSION_STRING}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

message(STATUS "Dependencies:")
function(show_dependency_status OUTPUT_NAME NAME)
  if(${NAME}_FOUND)
    if(${NAME}_BUNDLED)
      message(STATUS " * ${OUTPUT_NAME} not found (using bundled version)")
    else()
      message(STATUS " * ${OUTPUT_NAME} found")
    endif()
  else()
    message(STATUS " * ${OUTPUT_NAME} not found")
  endif()
endfunction()

show_dependency_status("Curl" CURL)
if(TARGET_OS AND TARGET_OS STREQUAL "mac")
  show_dependency_status("Dmg tools" DMGTOOLS)
endif()
if(CLIENT AND VIDEORECORDER)
  show_dependency_status("FFmpeg" FFMPEG)
endif()
show_dependency_status("Freetype" FREETYPE)
if(DOWNLOAD_GTEST)
  show_dependency_status("Git" GIT)
endif()
show_dependency_status("Glew" GLEW)
show_dependency_status("GTest" GTEST)
if(TARGET_OS AND TARGET_OS STREQUAL "mac")
  show_dependency_status("Dmgbuild" DMGBUILD)
endif()
if(UPNP)
  show_dependency_status("Miniupnpc" MINIUPNPC)
endif()
if(MYSQL)
  show_dependency_status("MySQL" MYSQL)
endif()
show_dependency_status("Ogg" OGG)
show_dependency_status("OpenSSL Crypto" CRYPTO)
show_dependency_status("Opus" OPUS)
show_dependency_status("Opusfile" OPUSFILE)
show_dependency_status("PNG" PNG)
show_dependency_status("Python3" Python3)
show_dependency_status("SDL2" SDL2)
show_dependency_status("SQLite3" SQLite3)
show_dependency_status("Wavpack" WAVPACK)
show_dependency_status("Zlib" ZLIB)
if(DISCORD)
  show_dependency_status("DiscordSdk" DiscordSdk)
endif()
if(WEBSOCKETS)
  show_dependency_status("Websockets" WEBSOCKETS)
endif()

if(CLIENT AND VULKAN)
  show_dependency_status("Vulkan" VULKAN)
endif()

if(NOT(CURL_FOUND))
  message(SEND_ERROR "You must install Curl to compile ${CMAKE_PROJECT_NAME}")
endif()
if(NOT(Python3_FOUND))
  message(SEND_ERROR "You must install Python to compile ${CMAKE_PROJECT_NAME}")
endif()
if(NOT(RUST_FOUND))
  message(SEND_ERROR "You must install Rust and Cargo to compile ${CMAKE_PROJECT_NAME}")
endif()
if(NOT(SQLite3_FOUND))
  message(SEND_ERROR "You must install SQLite3 to compile ${CMAKE_PROJECT_NAME}")
endif()

if(MYSQL AND NOT(MYSQL_FOUND))
  message(SEND_ERROR "You must install MySQL to compile the ${CMAKE_PROJECT_NAME} server with MySQL support")
endif()

if(WEBSOCKETS AND NOT(WEBSOCKETS_FOUND))
  message(SEND_ERROR "You must install libwebsockets to compile the ${CMAKE_PROJECT_NAME} server with websocket support")
endif()

if(UPNP AND NOT(MINIUPNPC_FOUND))
  message(SEND_ERROR "You must install miniupnpc to compile the ${CMAKE_PROJECT_NAME} server with UPnP support")
endif()
if(DISCORD AND NOT(DISCORDSDK_FOUND))
  message(SEND_ERROR "You must install the Discord SDK to compile the ${CMAKE_PROJECT_NAME} client with Discord support")
endif()
if(DISCORD_DYNAMIC)
  if(TARGET_OS STREQUAL "windows" OR TARGET_OS STREQUAL "mac")
    message(SEND_ERROR "Dynamically loading the Discord SDK is only supported on Linux")
  endif()
  if(NOT DISCORD)
    message(SEND_ERROR "You must enable the DISCORD flag if you want to link the Discord SDK")
  endif()
endif()
if(NOT(PNG_FOUND))
  message(SEND_ERROR "You must install libpng to compile ${CMAKE_PROJECT_NAME}")
endif()
if(CLIENT AND NOT(FREETYPE_FOUND))
  message(SEND_ERROR "You must install Freetype to compile the ${CMAKE_PROJECT_NAME} client")
endif()
if(CLIENT AND NOT(OGG_FOUND))
  message(SEND_ERROR "You must install Ogg to compile the ${CMAKE_PROJECT_NAME} client")
endif()
if(CLIENT AND NOT(OPUS_FOUND))
  message(SEND_ERROR "You must install Opus to compile the ${CMAKE_PROJECT_NAME} client")
endif()
if(CLIENT AND NOT(OPUSFILE_FOUND))
  message(SEND_ERROR "You must install Opusfile to compile the ${CMAKE_PROJECT_NAME} client")
endif()
if(CLIENT AND NOT(SDL2_FOUND))
  message(SEND_ERROR "You must install SDL2 to compile the ${CMAKE_PROJECT_NAME} client")
endif()
if(TARGET_OS STREQUAL "android" AND NOT(CRYPTO_FOUND))
  message(SEND_ERROR "You must install OpenSSL to compile ${CMAKE_PROJECT_NAME}")
endif()
if(NOT(GTEST_FOUND))
  if(DOWNLOAD_GTEST)
    if(GIT_FOUND)
      message(STATUS "Automatically downloading GTest to be able to run tests")
    else()
      set(DOWNLOAD_GTEST OFF)
      message(WARNING "To automatically download GTest, you have to install Git")
    endif()
  else()
    message(STATUS "To run the tests, you have to install GTest")
  endif()
endif()

if(CLIENT AND VULKAN AND NOT(VULKAN_FOUND))
  message(SEND_ERROR "You must install Vulkan libraries to compile the ${CMAKE_PROJECT_NAME} client")
endif()

if(TARGET_OS STREQUAL "windows")
  set(PLATFORM_CLIENT)
  set(PLATFORM_CLIENT_LIBS opengl32 winmm imm32)
  set(PLATFORM_LIBS)
  list(APPEND PLATFORM_LIBS shlwapi) # PathIsRelativeW
  list(APPEND PLATFORM_LIBS version ws2_32) # Windows sockets
  list(APPEND PLATFORM_LIBS bcrypt userenv) # for Rust (https://github.com/rust-lang/rust/issues/91974)
  list(APPEND PLATFORM_LIBS ole32) # CoInitialize(Ex)
  list(APPEND PLATFORM_LIBS shell32)
  list(APPEND PLATFORM_LIBS ntdll) # https://github.com/ddnet/ddnet/issues/6725
elseif(TARGET_OS STREQUAL "mac")
  find_library(CARBON Carbon)
  find_library(COCOA Cocoa)
  find_library(OPENGL OpenGL)
  find_library(SECURITY Security)
  set(PLATFORM_CLIENT
    src/macos/client.mm
    src/macos/notifications.mm
  )
  set(PLATFORM_CLIENT_LIBS ${COCOA} ${OPENGL})
  set(PLATFORM_LIBS ${CARBON} ${SECURITY})
elseif(TARGET_OS STREQUAL "haiku")
    set(PLATFORM_CLIENT)
    find_package(OpenGL)
    set(PLATFORM_LIBS GL network)
    set(PLATFORM_CLIENT_LIBS ${OPENGL_gl_LIBRARY})
    set(PLATFORM_CLIENT_INCLUDE_DIRS ${OPENGL_INCLUDE_DIR})
elseif(TARGET_OS STREQUAL "android")
    set(PLATFORM_CLIENT
      src/android/android_main.cpp
    )
    set(PLATFORM_LIBS ${TW_ANDROID_LIBS})
    set(PLATFORM_CLIENT_LIBS ${TW_ANDROID_LIBS_CLIENT})
    set(PLATFORM_CLIENT_INCLUDE_DIRS)
elseif(TARGET_OS STREQUAL "emscripten")
  find_package(OpenGL)
  set(PLATFORM_CLIENT_LIBS GL)
  set(PLATFORM_CLIENT_INCLUDE_DIRS "")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(PLATFORM_CLIENT)
  set(PLATFORM_LIBS)
  list(APPEND PLATFORM_LIBS rt) # clock_gettime for glibc < 2.17
  list(APPEND PLATFORM_LIBS dl) # for Rust
else()
  find_package(Notify)
  find_package(OpenGL)
  set(PLATFORM_CLIENT_LIBS ${OPENGL_gl_LIBRARY} ${NOTIFY_LIBRARIES})
  set(PLATFORM_CLIENT_INCLUDE_DIRS ${OPENGL_INCLUDE_DIR} ${NOTIFY_INCLUDE_DIRS})
  set(PLATFORM_CLIENT)
  if(TARGET_OS STREQUAL "linux")
    set(PLATFORM_LIBS)
    list(APPEND PLATFORM_LIBS rt) # clock_gettime for glibc < 2.17
    list(APPEND PLATFORM_LIBS dl) # for Rust
  else()
    set(PLATFORM_LIBS)
  endif()
endif()

########################################################################
# DOWNLOAD GTEST
########################################################################

if(NOT(GTEST_FOUND) AND DOWNLOAD_GTEST)
  set(DDNET_GTEST_VERSION 3d73dee972d0db344bda9b659836612aba6a3564)
  configure_file(cmake/Download_GTest_CMakeLists.txt.in googletest-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/googletest-download
  )
  if(result)
    message(WARNING "CMake step for googletest failed: ${result}")
    set(DOWNLOAD_GTEST OFF)
  else()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/googletest-download
    )
    if(result)
      message(WARNING "Build step for googletest failed: ${result}")
      set(DOWNLOAD_GTEST OFF)
    else()
      # Prevent overriding the parent project's compiler/linker settings on Windows
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

      # Add googletest directly to our build. This defines the gtest target.
      add_subdirectory(
        ${PROJECT_BINARY_DIR}/googletest-src
        ${PROJECT_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL
      )

      if(MSVC)
        foreach(target gtest gmock)
          set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded$<${DBG}:Debug>)
          # `/w` disables all warnings. This is needed because `gtest` enables
          # `/WX` (equivalent of `-Werror`) for some reason, breaking builds
          # when MSVS adds new warnings.
          target_compile_options(${target} PRIVATE /w)
        endforeach()
      endif()

      set(GTEST_LIBRARIES gtest gmock)
      set(GTEST_INCLUDE_DIRS)
    endif()
  endif()
endif()

########################################################################
# DEPENDENCY COMPILATION
########################################################################

set_src(DEP_JSON_SRC GLOB src/engine/external/json-parser json.c json.h)
add_library(json EXCLUDE_FROM_ALL OBJECT ${DEP_JSON_SRC})

set_src(DEP_MD5_SRC GLOB src/engine/external/md5 md5.c md5.h)
add_library(md5 EXCLUDE_FROM_ALL OBJECT ${DEP_MD5_SRC})

list(APPEND TARGETS_DEP json md5)
set(DEP_JSON $<TARGET_OBJECTS:json>)
set(DEP_MD5)
if(NOT CRYPTO_FOUND)
  set(DEP_MD5 $<TARGET_OBJECTS:md5>)
endif()

########################################################################
# RUST
########################################################################

set_glob(RUST_BASE GLOB_RECURSE "rs;toml" src/base)

set_glob(RUST_ENGINE_INTERFACE GLOB "rs;toml" src/engine)

set_glob(RUST_ENGINE_SHARED GLOB_RECURSE "rs;toml" src/engine/shared)

set_src(RUST_BRIDGE_SHARED GLOB_RECURSE src/rust-bridge)

set_glob(RUST_MASTERSRV GLOB "rs;toml" src/mastersrv/src)

add_library(rust-bridge-shared EXCLUDE_FROM_ALL OBJECT ${RUST_BRIDGE_SHARED})
list(APPEND TARGETS_OWN rust-bridge-shared)

if(TARGET_OS STREQUAL "android")
  set(CARGO_BUILD_DIR "${CARGO_NDK_TARGET}/")
  set(CARGO_BUILD ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${PROJECT_BINARY_DIR} DDNET_TEST_NO_LINK=1 ${RUST_CARGO} ndk --manifest-path "${PROJECT_SOURCE_DIR}/Cargo.toml" -t ${CARGO_NDK_TARGET} -p ${CARGO_NDK_API} build)
  set(CARGO_TEST ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${PROJECT_BINARY_DIR} ${RUST_CARGO} ndk -t ${CARGO_NDK_TARGET} -p ${CARGO_NDK_API} test)
else()
  set(CARGO_BUILD_DIR "")
  set(CARGO_BUILD ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${PROJECT_BINARY_DIR} DDNET_TEST_NO_LINK=1 ${RUST_CARGO} build --manifest-path "${PROJECT_SOURCE_DIR}/Cargo.toml")
  set(CARGO_TEST ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${PROJECT_BINARY_DIR} ${RUST_CARGO} test)
endif()
if(MSVC)
  list(INSERT CARGO_BUILD 0 ${CMAKE_COMMAND} -E env $<$<CONFIG:Debug>:CFLAGS=/MTd> $<$<CONFIG:Debug>:CXXFLAGS=/MTd>)
  list(INSERT CARGO_TEST 0 ${CMAKE_COMMAND} -E env RUSTFLAGS=-Ctarget-feature=+crt-static)
endif()
if(RUST_NIGHTLY)
  list(APPEND CARGO_BUILD -Z build-std=std,panic_abort)
endif()
if(NOT CMAKE_OSX_ARCHITECTURES AND (DEFINED CMAKE_RUST_COMPILER_TARGET OR RUST_NIGHTLY))
  if(DEFINED CMAKE_RUST_COMPILER_TARGET)
    list(APPEND CARGO_TEST --target ${CMAKE_RUST_COMPILER_TARGET})
    set(RUST_TARGET ${CMAKE_RUST_COMPILER_TARGET})
  else()
    set(RUST_TARGET ${RUST_TARGET_HOST})
  endif()
  list(APPEND CARGO_BUILD --target ${RUST_TARGET})
  set(CARGO_BUILD_DIR "${RUST_TARGET}/")
endif()
set(CARGO_BUILD_DIR_DEBUG "${CARGO_BUILD_DIR}debug")
set(CARGO_BUILD_DIR_RELEASE "${CARGO_BUILD_DIR}release")
if(GENERATOR_IS_MULTI_CONFIG)
  if(CMAKE_VERSION VERSION_LESS 3.20)
    message(SEND_ERROR "Multi-config generators only supported from CMake 3.20 and up")
  else()
    set(CARGO_BUILD_DIR "${CARGO_BUILD_DIR}$<$<CONFIG:Debug>:debug>$<$<NOT:$<CONFIG:Debug>>:release>")
  endif()
else()
  if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CARGO_BUILD_DIR "${CARGO_BUILD_DIR_DEBUG}")
  else()
    set(CARGO_BUILD_DIR "${CARGO_BUILD_DIR_RELEASE}")
  endif()
endif()
list(APPEND CARGO_BUILD $<$<NOT:$<CONFIG:Debug>>:--release>)

if(CMAKE_OSX_ARCHITECTURES)
  set(RUST_OSX_ARCHITECTURES)
  foreach(arch ${CMAKE_OSX_ARCHITECTURES})
    if(${arch} STREQUAL arm64)
      list(APPEND RUST_OSX_ARCHITECTURES aarch64-apple-darwin)
    elseif(${arch} STREQUAL x86_64)
      list(APPEND RUST_OSX_ARCHITECTURES x86_64-apple-darwin)
    else()
      message(SEND_ERROR "CMAKE_OSX_ARCHITECTURES' architecture ${arch} unknown, can't build Rust code (known: arm64, x86_64)")
    endif()
  endforeach()
endif()

set(RUST_SRC
      ${RUST_BASE}
      ${RUST_ENGINE_INTERFACE}
      ${RUST_ENGINE_SHARED}
      Cargo.toml
      Cargo.lock
)
set(RUST_TARGETS engine_shared)
if(NOT CMAKE_OSX_ARCHITECTURES)
  set(RUST_OUTPUTS)
  foreach(rust_target ${RUST_TARGETS})
    set(LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}ddnet_${rust_target}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    add_library(rust_${rust_target} STATIC IMPORTED GLOBAL)
    add_custom_target(rust_${rust_target}_target DEPENDS "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
    add_dependencies(rust_${rust_target} rust_${rust_target}_target)
    set_target_properties(rust_${rust_target} PROPERTIES
      IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_RELEASE}/${LIBRARY_NAME}"
      IMPORTED_LOCATION_DEBUG "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_DEBUG}/${LIBRARY_NAME}"
    )
    list(APPEND RUST_OUTPUTS "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
  endforeach()
  add_custom_command(
    OUTPUT ${RUST_OUTPUTS}
    COMMAND ${CARGO_BUILD}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    USES_TERMINAL
    DEPENDS ${RUST_SRC}
  )
else()
  foreach(rust_target ${RUST_TARGETS})
    set(LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}ddnet_${rust_target}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    add_library(rust_${rust_target} STATIC IMPORTED GLOBAL)
    set_target_properties(rust_${rust_target} PROPERTIES
      IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_RELEASE}/${LIBRARY_NAME}"
      IMPORTED_LOCATION_DEBUG "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_DEBUG}/${LIBRARY_NAME}"
    )
    add_custom_target(rust_${rust_target}_target DEPENDS "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
    add_dependencies(rust_${rust_target} rust_${rust_target}_target)
    set(ARCH_LIBRARIES)
    foreach(arch ${RUST_OSX_ARCHITECTURES})
      list(APPEND ARCH_LIBRARIES "${PROJECT_BINARY_DIR}/${arch}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
    endforeach()
    add_custom_command(
      OUTPUT "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}"
      COMMAND lipo ${ARCH_LIBRARIES} -create -output "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}"
      DEPENDS ${ARCH_LIBRARIES}
    )
  endforeach()
  foreach(arch ${RUST_OSX_ARCHITECTURES})
    set(RUST_OUTPUTS)
    foreach(rust_target ${RUST_TARGETS})
      set(LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}ddnet_${rust_target}${CMAKE_STATIC_LIBRARY_SUFFIX}")
      list(APPEND RUST_OUTPUTS "${PROJECT_BINARY_DIR}/${arch}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
    endforeach()
    add_custom_command(
      OUTPUT ${RUST_OUTPUTS}
      COMMAND ${CARGO_BUILD} --target=${arch}
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
      USES_TERMINAL
      DEPENDS ${RUST_SRC}
    )
  endforeach()
endif()

########################################################################
# DATA
########################################################################

set_glob(DATA GLOB_RECURSE "cfg;frag;json;map;otf;png;rules;ttc;ttf;txt;vert;wv" data)

########################################################################
# COPY DATA AND SHARED LIBS
########################################################################

foreach(datafile ${DATA})
  file(RELATIVE_PATH OUT ${PROJECT_SOURCE_DIR}/data ${datafile})
  get_filename_component(DESTINATION data/${OUT} PATH)
  file(MAKE_DIRECTORY ${DESTINATION})
  file(COPY ${datafile} DESTINATION ${DESTINATION})
endforeach()
set(COPY_FILES
  ${CURL_COPY_FILES}
  ${FREETYPE_COPY_FILES}
  ${OPUSFILE_COPY_FILES}
  ${PNG_COPY_FILES}
  ${SDL2_COPY_FILES}
  ${SQLite3_COPY_FILES}
  ${FFMPEG_COPY_FILES}
  ${WEBSOCKETS_COPY_FILES}
  ${DISCORDSDK_COPY_FILES}
  ${VULKAN_COPY_FILES}
  ${CRASHDUMP_COPY_FILES}
  ${SSP_COPY_FILES}
)
file(COPY ${COPY_FILES} DESTINATION .)

set(COPY_DIRS ${SDL2_COPY_DIRS})
file(COPY ${COPY_DIRS} DESTINATION .)

########################################################################
# CODE GENERATION
########################################################################

function(generate_source output_file script_parameter)
  add_custom_command(OUTPUT ${output_file}
    COMMAND ${Python3_EXECUTABLE} datasrc/compile.py ${script_parameter}
      > "${PROJECT_BINARY_DIR}/${output_file}"
    DEPENDS
      datasrc/compile.py
      datasrc/content.py
      datasrc/datatypes.py
      datasrc/network.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endfunction()

function(generate_source7 output_file script_parameter)
  add_custom_command(OUTPUT ${output_file}
    COMMAND ${Python3_EXECUTABLE} -m datasrc.seven.compile ${script_parameter}
      > "${PROJECT_BINARY_DIR}/${output_file}"
    DEPENDS
      datasrc/seven/compile.py
      datasrc/seven/content.py
      datasrc/seven/datatypes.py
      datasrc/seven/network.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endfunction()

function(generate_maps output_file script_parameter)
  add_custom_command(OUTPUT ${output_file}
    COMMAND ${Python3_EXECUTABLE} datasrc/crosscompile.py ${script_parameter}
      > "${PROJECT_BINARY_DIR}/${output_file}"
    DEPENDS
      datasrc/crosscompile.py
      datasrc/compile.py
      datasrc/content.py
      datasrc/datatypes.py
      datasrc/network.py
      datasrc/seven/compile.py
      datasrc/seven/content.py
      datasrc/seven/datatypes.py
      datasrc/seven/network.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endfunction()

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src/game/generated/")
execute_process(COMMAND git rev-parse --git-dir
  ERROR_QUIET
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  OUTPUT_VARIABLE PROJECT_GIT_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE PROJECT_GIT_DIR_ERROR
)
if(NOT PROJECT_GIT_DIR_ERROR)
  set(GIT_REVISION_EXTRA_DEPS
    ${PROJECT_GIT_DIR}/index
    ${PROJECT_GIT_DIR}/logs/HEAD
  )
endif()
add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/src/game/generated/git_revision.cpp
  COMMAND ${Python3_EXECUTABLE}
    scripts/git_revision.py
    > ${PROJECT_BINARY_DIR}/src/game/generated/git_revision.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  DEPENDS
    ${GIT_REVISION_EXTRA_DEPS}
    scripts/git_revision.py
)
generate_source("src/game/generated/client_data.cpp" "client_content_source")
generate_source("src/game/generated/client_data.h" "client_content_header")
generate_source("src/game/generated/data_types.h" "content_types_header")
generate_source("src/game/generated/protocol.cpp" "network_source")
generate_source("src/game/generated/protocol.h" "network_header")
generate_source("src/game/generated/server_data.cpp" "server_content_source")
generate_source("src/game/generated/server_data.h" "server_content_header")

generate_source7("src/game/generated/protocol7.cpp" "network_source")
generate_source7("src/game/generated/protocol7.h" "network_header")
generate_source7("src/game/generated/client_data7.cpp" "client_content_source")
generate_source7("src/game/generated/client_data7.h" "client_content_header")

generate_maps("src/game/generated/protocolglue.h" "map_header")
generate_maps("src/game/generated/protocolglue.cpp" "map_source")

add_custom_command(OUTPUT "src/game/generated/wordlist.h"
  COMMAND ${Python3_EXECUTABLE} scripts/wordlist.py > ${PROJECT_BINARY_DIR}/src/game/generated/wordlist.h
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  DEPENDS
    scripts/wordlist.py
)

########################################################################
# SHARED
########################################################################

# Sources
set_src(BASE GLOB_RECURSE src/base)
set_src(ENGINE_INTERFACE GLOB src/engine)
set_src(ENGINE_SHARED GLOB_RECURSE src/engine/shared)
set_src(ENGINE_GFX GLOB src/engine/gfx)
set_src(GAME_SHARED GLOB src/game)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "src/game/version.h")

# A bit hacky, but these are needed to register all the UUIDs, even for stuff
# that doesn't link game.
set(ENGINE_UUID_SHARED
  src/game/generated/protocolglue.cpp
  src/game/generated/protocolglue.h
  src/game/generated/protocol7.cpp
  src/game/generated/protocol7.h
  src/game/generated/protocol.cpp
  src/game/generated/protocol.h
  src/game/mapitems_ex.cpp
  src/game/mapitems_ex.h
  src/game/mapitems_ex_types.h
)
foreach(s ${GAME_SHARED})
  if(s MATCHES "mapitems_(ex.cpp|ex.h|ex_types.h)$")
    list(REMOVE_ITEM GAME_SHARED ${s})
  endif()
endforeach()
list(REMOVE_ITEM GAME_SHARED ${ENGINE_UUID_SHARED})
set(GAME_GENERATED_SHARED
  src/game/generated/data_types.h
  src/game/generated/git_revision.cpp
  src/game/generated/protocol.h
  src/game/generated/protocol7.h
  src/game/generated/protocolglue.h
)
set(DEPS ${DEP_JSON} ${DEP_MD5} ${ZLIB_DEP})

# Libraries
set(LIBS
  ${CRYPTO_LIBRARIES}
  ${CURL_LIBRARIES}
  ${SQLite3_LIBRARIES}
  ${WEBSOCKETS_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${PLATFORM_LIBS}
  # Add pthreads (on non-Windows) at the end, so that other libraries can depend
  # on it.
  ${CMAKE_THREAD_LIBS_INIT}
)

# Targets
add_library(engine-gfx EXCLUDE_FROM_ALL OBJECT ${ENGINE_GFX})
target_include_directories(engine-gfx PRIVATE ${PNG_INCLUDE_DIRS})
add_library(engine-shared EXCLUDE_FROM_ALL OBJECT ${ENGINE_INTERFACE} ${ENGINE_SHARED} ${ENGINE_UUID_SHARED} ${BASE})
add_library(game-shared EXCLUDE_FROM_ALL OBJECT ${GAME_SHARED} ${GAME_GENERATED_SHARED})
list(APPEND TARGETS_OWN engine-gfx engine-shared game-shared)

if(DISCORD AND NOT DISCORD_DYNAMIC)
  add_library(discord-shared SHARED IMPORTED)
  set_target_properties(discord-shared PROPERTIES
    IMPORTED_LOCATION "${DISCORDSDK_LIBRARIES}"
    IMPORTED_IMPLIB "${DISCORDSDK_LIBRARIES}"
  )
endif()

########################################################################
# CLIENT
########################################################################

if(CLIENT)
  # Sources
  set_src(STEAMAPI_SRC GLOB_RECURSE src/steam)

  if(STEAM OR TARGET_OS STREQUAL "windows" OR TARGET_OS STREQUAL "mac")
    set(STEAMAPI_KIND SHARED)
  else()
    set(STEAMAPI_KIND STATIC)
  endif()
  set(TARGET_STEAMAPI steam_api)
  add_library(${TARGET_STEAMAPI} ${STEAMAPI_KIND} ${STEAMAPI_SRC})
  list(APPEND TARGETS_OWN ${TARGET_STEAMAPI})

  set_src(ENGINE_CLIENT GLOB_RECURSE src/engine/client)
  set_src(GAME_CLIENT GLOB_RECURSE src/game/client)
  set_src(GAME_EDITOR GLOB_RECURSE src/game/editor)
  set(GAME_GENERATED_CLIENT
    src/game/generated/checksum.cpp
    src/game/generated/client_data.cpp
    src/game/generated/client_data.h
    src/game/generated/client_data7.cpp
    src/game/generated/client_data7.h
  )
  set(CLIENT_SRC ${ENGINE_CLIENT} ${PLATFORM_CLIENT} ${GAME_CLIENT} ${GAME_EDITOR} ${GAME_GENERATED_CLIENT})

  set(DEPS_CLIENT ${DEPS} ${GLEW_DEP} ${WAVPACK_DEP})

  # Libraries
  set(LIBS_CLIENT
    ${FREETYPE_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${WAVPACK_LIBRARIES}
    ${FFMPEG_LIBRARIES}

    # Order of these three is important.
    ${OPUSFILE_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${OGG_LIBRARIES}

    ${VULKAN_LIBRARIES}

    ${TARGET_STEAMAPI}
    rust_engine_shared

    ${PLATFORM_CLIENT_LIBS}
    ${LIBS}
  )

  if(DISCORD)
    if(NOT DISCORD_DYNAMIC)
      list(APPEND LIBS_CLIENT discord-shared)
    else()
      list(APPEND LIBS_CLIENT ${CMAKE_DL_LIBS})
    endif()
  endif()

  if(TARGET_OS STREQUAL "windows")
    set(VERSION_EXECUTABLE "${CLIENT_EXECUTABLE}")
    set(VERSION_COMPANY "${CLIENT_EXECUTABLE} Team")
    configure_file("other/versioninfo/versioninfo.rc.in" "client.versioninfo.rc")
    configure_file("other/manifest/client.manifest.in" "client.manifest")
    set(CLIENT_ICON "other/icons/DDNet.rc")
    set(CLIENT_VERSIONINFO "${CMAKE_CURRENT_BINARY_DIR}/client.versioninfo.rc")
    if(NOT MINGW)
      set(CLIENT_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/client.manifest")
    else()
      set(CLIENT_MANIFEST "other/manifest/client.rc")
      set_target_properties(${TARGET_STEAMAPI} PROPERTIES PREFIX "")
    endif()
  else()
    set(CLIENT_ICON)
    set(CLIENT_VERSIONINFO)
    set(CLIENT_MANIFEST)
  endif()

  # Target
  if(TARGET_OS STREQUAL "android")
    add_library(game-client SHARED
      ${CLIENT_SRC}
      ${CLIENT_ICON}
      ${CLIENT_VERSIONINFO}
      ${CLIENT_MANIFEST}
      ${DEPS_CLIENT}
      $<TARGET_OBJECTS:engine-gfx>
      $<TARGET_OBJECTS:engine-shared>
      $<TARGET_OBJECTS:game-shared>
      $<TARGET_OBJECTS:rust-bridge-shared>
    )
  else()
    add_executable(game-client WIN32
      ${CLIENT_SRC}
      ${CLIENT_ICON}
      ${CLIENT_VERSIONINFO}
      ${CLIENT_MANIFEST}
      ${DEPS_CLIENT}
      $<TARGET_OBJECTS:engine-gfx>
      $<TARGET_OBJECTS:engine-shared>
      $<TARGET_OBJECTS:game-shared>
      $<TARGET_OBJECTS:rust-bridge-shared>
    )
  endif()
  set_property(TARGET game-client
      PROPERTY OUTPUT_NAME ${CLIENT_EXECUTABLE}
  )
  target_link_libraries(game-client ${LIBS_CLIENT})

  if(MSVC)
    target_link_options(game-client PRIVATE /ENTRY:mainCRTStartup)
  endif()

  target_include_directories(game-client SYSTEM PRIVATE
    ${FREETYPE_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${OPUSFILE_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${WAVPACK_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${DISCORDSDK_INCLUDE_DIRS}

    ${VULKAN_INCLUDE_DIRS}

    ${PLATFORM_CLIENT_INCLUDE_DIRS}
  )

  if(STEAMAPI_KIND STREQUAL SHARED OR DISCORD_DYNAMIC)
    set_own_rpath(game-client)
  endif()

  set(PARAMS "${WAVPACK_INCLUDE_DIRS};${WAVPACK_INCLUDE_DIRS}")
  if(NOT(WAVPACK_OPEN_FILE_INPUT_EX_PARAMS STREQUAL PARAMS))
    unset(WAVPACK_OPEN_FILE_INPUT_EX CACHE)
  endif()
  set(WAVPACK_OPEN_FILE_INPUT_EX_PARAMS "${PARAMS}" CACHE INTERNAL "")

  set(CMAKE_REQUIRED_INCLUDES ${ORIGINAL_CMAKE_REQUIRED_INCLUDES} ${WAVPACK_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${ORIGINAL_CMAKE_REQUIRED_LIBRARIES} ${WAVPACK_LIBRARIES})
  check_symbol_exists(WavpackOpenFileInputEx wavpack.h WAVPACK_OPEN_FILE_INPUT_EX)
  check_symbol_exists(WavpackCloseFile wavpack.h WAVPACK_CLOSE_FILE)
  set(CMAKE_REQUIRED_INCLUDES ${ORIGINAL_CMAKE_REQUIRED_INCLUDES})
  set(CMAKE_REQUIRED_LIBRARIES ${ORIGINAL_CMAKE_REQUIRED_LIBRARIES})

  if(WAVPACK_OPEN_FILE_INPUT_EX)
    target_compile_definitions(game-client PRIVATE CONF_WAVPACK_OPEN_FILE_INPUT_EX)
  endif()

  if(WAVPACK_CLOSE_FILE)
    target_compile_definitions(game-client PRIVATE CONF_WAVPACK_CLOSE_FILE)
  endif()

  if(GLEW_BUNDLED)
    target_compile_definitions(game-client PRIVATE CONF_GLEW_HAS_CONTEXT_INIT)
  endif()

  if(VULKAN)
    target_compile_definitions(game-client PRIVATE CONF_BACKEND_VULKAN)
  endif()

  list(APPEND TARGETS_OWN game-client)
  list(APPEND TARGETS_LINK game-client)
endif()


########################################################################
# SERVER
########################################################################

if(SERVER)
  # Sources
  set_src(ANTIBOT_SRC GLOB src/antibot)

  set_src(ENGINE_SERVER_WITHOUT_MAIN GLOB_RECURSE src/engine/server)

  list(REMOVE_ITEM ENGINE_SERVER_WITHOUT_MAIN "${PROJECT_SOURCE_DIR}/src/engine/server/main.cpp")

  set_src(GAME_SERVER GLOB_RECURSE src/game/server)
  set(GAME_GENERATED_SERVER
    "src/game/generated/server_data.cpp"
    "src/game/generated/server_data.h"
    "src/game/generated/wordlist.h"
  )
  set(SERVER_SRC ${ENGINE_SERVER_WITHOUT_MAIN} ${GAME_SERVER} ${GAME_GENERATED_SERVER})
  if(TARGET_OS STREQUAL "windows")
    set(VERSION_EXECUTABLE "${SERVER_EXECUTABLE}")
    set(VERSION_COMPANY "${CLIENT_EXECUTABLE} Team")
    configure_file("other/versioninfo/versioninfo.rc.in" "server.versioninfo.rc")
    set(SERVER_ICON "other/icons/DDNet-Server.rc")
    set(SERVER_VERSIONINFO "${CMAKE_CURRENT_BINARY_DIR}/server.versioninfo.rc")
  else()
    set(SERVER_ICON)
    set(SERVER_VERSIONINFO)
  endif()

  # Antibot
  if(ANTIBOT)
    set(TARGET_ANTIBOT antibot)
    add_library(${TARGET_ANTIBOT} SHARED ${ANTIBOT_SRC})
    list(APPEND TARGETS_OWN ${TARGET_ANTIBOT})
  endif()

  # Libraries
  set(LIBS_SERVER
    ${MINIUPNPC_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${TARGET_ANTIBOT}
    rust_engine_shared

    ${LIBS}
  )

  add_library(game-server-without-main OBJECT
      ${DEPS}
      ${SERVER_SRC}
      $<TARGET_OBJECTS:engine-shared>
      $<TARGET_OBJECTS:game-shared>
      $<TARGET_OBJECTS:rust-bridge-shared>
  )
  set_property(TARGET game-server-without-main
      PROPERTY OUTPUT_NAME ${SERVER_EXECUTABLE}
  )
  target_link_libraries(game-server-without-main ${LIBS_SERVER})
  list(APPEND TARGETS_OWN game-server-without-main)
  list(APPEND TARGETS_LINK game-server-without-main)

  # Target
  if(TARGET_OS STREQUAL "android")
    add_library(game-server SHARED
      ${DEPS}
      $<TARGET_OBJECTS:game-server-without-main>
      "${PROJECT_SOURCE_DIR}/src/engine/server/main.cpp"
      ${SERVER_ICON}
      ${SERVER_VERSIONINFO}
      $<TARGET_OBJECTS:engine-shared>
      $<TARGET_OBJECTS:game-shared>
      $<TARGET_OBJECTS:rust-bridge-shared>
    )
  else()
    add_executable(game-server
      ${DEPS}
      "${PROJECT_SOURCE_DIR}/src/engine/server/main.cpp"
      ${SERVER_ICON}
      ${SERVER_VERSIONINFO}
      $<TARGET_OBJECTS:game-server-without-main>
      $<TARGET_OBJECTS:engine-shared>
      $<TARGET_OBJECTS:game-shared>
      $<TARGET_OBJECTS:rust-bridge-shared>
    )
  endif()
  set_property(TARGET game-server
      PROPERTY OUTPUT_NAME ${SERVER_EXECUTABLE}
  )
  target_link_libraries(game-server ${LIBS_SERVER})
  list(APPEND TARGETS_OWN game-server)
  list(APPEND TARGETS_LINK game-server)

  if(TARGET_OS AND TARGET_OS STREQUAL "mac")
    set(SERVER_LAUNCHER_SRC src/macos/server.mm)
    add_executable(game-server-launcher ${SERVER_LAUNCHER_SRC})
    set_property(TARGET game-server-launcher
        PROPERTY OUTPUT_NAME ${SERVER_EXECUTABLE}-Launcher
    )
    target_link_libraries(game-server-launcher ${COCOA})
    list(APPEND TARGETS_OWN game-server-launcher)
    list(APPEND TARGETS_LINK game-server-launcher)
  endif()
endif()

# Targets for compatibility with build commands previously available with Makefiles
if(CMAKE_GENERATOR MATCHES ".*Makefiles.*")
  if(TARGET game-client)
    add_custom_target(${CLIENT_EXECUTABLE})
    add_dependencies(${CLIENT_EXECUTABLE} game-client)
  endif()

  if(TARGET game-server)
    add_custom_target(${SERVER_EXECUTABLE})
    add_dependencies(${SERVER_EXECUTABLE} game-server)
  endif()
endif()

########################################################################
# VARIOUS TARGETS
########################################################################

if(TOOLS)
  set(TARGETS_TOOLS)
  set_src(TOOLS_SRC GLOB src/tools)
  foreach(ABS_T ${TOOLS_SRC})
    file(RELATIVE_PATH T "${PROJECT_SOURCE_DIR}/src/tools/" ${ABS_T})
    if(T MATCHES "\\.cpp$")
      string(REGEX REPLACE "\\.cpp$" "" TOOL "${T}")
      set(TOOL_DEPS ${DEPS})
      set(TOOL_LIBS ${LIBS})
      unset(EXTRA_TOOL_SRC)
      if(TOOL MATCHES "^(dilate|map_convert_07|map_optimize|map_extract|map_replace_image)$")
        list(APPEND TOOL_INCLUDE_DIRS ${PNG_INCLUDE_DIRS})
        list(APPEND TOOL_DEPS $<TARGET_OBJECTS:engine-gfx>)
        list(APPEND TOOL_LIBS ${PNG_LIBRARIES})
      endif()
      if(TOOL MATCHES "^config_")
        list(APPEND EXTRA_TOOL_SRC "src/tools/config_common.h")
      endif()
      set(EXCLUDE_FROM_ALL)
      if(DEV)
        set(EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
      endif()
      add_executable(${TOOL} ${EXCLUDE_FROM_ALL}
        ${TOOL_DEPS}
        src/tools/${TOOL}.cpp
        ${EXTRA_TOOL_SRC}
        $<TARGET_OBJECTS:engine-shared>
        $<TARGET_OBJECTS:game-shared>
      )
      target_include_directories(${TOOL} SYSTEM PRIVATE ${TOOL_INCLUDE_DIRS})
      target_link_libraries(${TOOL} ${TOOL_LIBS})
      list(APPEND TARGETS_TOOLS ${TOOL})
    endif()
  endforeach()

  list(APPEND TARGETS_OWN ${TARGETS_TOOLS})
  list(APPEND TARGETS_LINK ${TARGETS_TOOLS})

  add_custom_target(tools DEPENDS ${TARGETS_TOOLS})
endif()
add_custom_target(everything DEPENDS ${TARGETS_OWN})

########################################################################
# CHECKSUM
########################################################################

if(DEV)
  # Only do minimal checksumming in a DEV build.
  set(CHECKSUM_SRC)
endif()
list(APPEND CHECKSUM_SRC
  ${PROJECT_SOURCE_DIR}/CMakeLists.txt
  ${PROJECT_SOURCE_DIR}/scripts/checksum.py
)
configure_file(cmake/checksummed_extra.txt checksummed_extra.txt)
string(REPLACE ";" "\n" CHECKSUM_SRC_FILE "${CHECKSUM_SRC}")
file(WRITE ${PROJECT_BINARY_DIR}/checksummed_files.txt ${CHECKSUM_SRC_FILE})

add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/src/game/generated/checksum.cpp
  COMMAND ${Python3_EXECUTABLE}
    scripts/checksum.py
    ${PROJECT_BINARY_DIR}/checksummed_files.txt
    ${PROJECT_BINARY_DIR}/checksummed_extra.txt
    > ${PROJECT_BINARY_DIR}/src/game/generated/checksum.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  DEPENDS
    ${CHECKSUM_SRC}
    ${PROJECT_BINARY_DIR}/checksummed_files.txt
    ${PROJECT_BINARY_DIR}/checksummed_extra.txt
    scripts/checksum.py
)

########################################################################
# TESTS
########################################################################

if((GTEST_FOUND OR DOWNLOAD_GTEST) AND SERVER)
  set_src(TESTS GLOB src/test)
  set(TESTS_EXTRA
    src/engine/client/blocklist_driver.cpp
    src/engine/client/blocklist_driver.h
    src/engine/client/serverbrowser.cpp
    src/engine/client/serverbrowser.h
    src/engine/client/serverbrowser_http.cpp
    src/engine/client/serverbrowser_http.h
    src/engine/client/serverbrowser_ping_cache.cpp
    src/engine/client/serverbrowser_ping_cache.h
    src/engine/client/sqlite.cpp
  )

  set(TARGET_TESTRUNNER testrunner)
  add_executable(${TARGET_TESTRUNNER} EXCLUDE_FROM_ALL
    ${TESTS}
    ${TESTS_EXTRA}
    $<TARGET_OBJECTS:game-server-without-main>
    $<TARGET_OBJECTS:engine-gfx>
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
    $<TARGET_OBJECTS:rust-bridge-shared>
    ${DEPS}
  )
  target_link_libraries(${TARGET_TESTRUNNER} ${PNG_LIBRARIES} ${GTEST_LIBRARIES} ${LIBS_SERVER})
  target_include_directories(${TARGET_TESTRUNNER} SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})

  list(APPEND TARGETS_OWN ${TARGET_TESTRUNNER})
  list(APPEND TARGETS_LINK ${TARGET_TESTRUNNER})

  add_custom_target(run_cxx_tests
    COMMAND $<TARGET_FILE:${TARGET_TESTRUNNER}> ${TESTRUNNER_ARGS}
    COMMENT Running unit tests
    DEPENDS ${TARGET_TESTRUNNER}
    USES_TERMINAL
  )
  add_custom_target(run_tests
    DEPENDS run_cxx_tests
  )
  if(NOT MSVC OR CMAKE_BUILD_TYPE STREQUAL Release)
    # On MSVC, Rust tests only work in the release mode because we link our C++
    # code with the debug C standard library (/MTd) but Rust only supports
    # linking to the release C standard library (/MT).
    #
    # See also https://github.com/rust-lang/rust/issues/39016.
    add_dependencies(run_tests run_rust_tests)
  endif()
endif()

add_library(rust_test STATIC EXCLUDE_FROM_ALL
  $<TARGET_OBJECTS:engine-gfx>
  $<TARGET_OBJECTS:engine-shared>
  $<TARGET_OBJECTS:game-shared>
  $<TARGET_OBJECTS:rust-bridge-shared>
  ${DEPS}
)

list(APPEND TARGETS_OWN rust_test)
list(APPEND TARGETS_LINK rust_test)

set(RUST_TEST_LIBS ${LIBS} $<TARGET_FILE:rust_test>)
list(REMOVE_ITEM RUST_TEST_LIBS "-pthread")
add_custom_target(run_rust_tests
  COMMAND ${CMAKE_COMMAND} -E env "DDNET_TEST_LIBRARIES=${RUST_TEST_LIBS}" ${CARGO_TEST}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  USES_TERMINAL
  DEPENDS rust_test
  VERBATIM
)

########################################################################
# INSTALLATION
########################################################################

function(escape_regex VAR STRING)
  string(REGEX REPLACE "([][^$.+*?|()\\\\])" "\\\\\\1" ESCAPED "${STRING}")
  set(${VAR} ${ESCAPED} PARENT_SCOPE)
endfunction()

function(escape_backslashes VAR STRING)
  string(REGEX REPLACE "\\\\" "\\\\\\\\" ESCAPED "${STRING}")
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
set(CPACK_GENERATOR TGZ TXZ)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
if(TARGET_OS STREQUAL "mac")
  set(CPACK_STRIP_FILES FALSE)
elseif(TARGET_OS STREQUAL "windows" AND TARGET_CPU_ARCHITECTURE STREQUAL "arm64")
  set(CPACK_STRIP_FILES FALSE)
else()
  set(CPACK_STRIP_FILES TRUE)
endif()
set(CPACK_COMPONENTS_ALL portable)
set(CPACK_SOURCE_GENERATOR ZIP TGZ TBZ2 TXZ)
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})

if(TARGET_OS AND TARGET_BITS)
  if(TARGET_OS STREQUAL "windows")
    if (TARGET_CPU_ARCHITECTURE STREQUAL "arm64")
      set(CPACK_SYSTEM_NAME "win-arm64")
    else()
      set(CPACK_SYSTEM_NAME "win${TARGET_BITS}")
    endif()
    set(CPACK_GENERATOR ZIP)
  elseif(TARGET_OS STREQUAL "linux")
    # Let compiler tell its arch
    # Both gcc and clang support -dumpmachine
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} -dumpmachine
      OUTPUT_VARIABLE ARCHITECTURE_TUPLE
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT ARCHITECTURE_TUPLE)
      # If you're really using a weird compiler, then assume Intel here.
      message(WARNING "Your compiler doesn't support -dumpmachine, this is weird")
      if(TARGET_BITS EQUAL 32)
        set(ARCHITECTURE "x86")
      elseif(TARGET_BITS EQUAL 64)
        set(ARCHITECTURE "x86_64")
      endif()
    else()
      string(REGEX MATCH "^[^-]*" ARCHITECTURE "${ARCHITECTURE_TUPLE}")
      if(ARCHITECTURE MATCHES "i.86")
        set(ARCHITECTURE "x86")
      endif()
    endif()
    set(CPACK_SYSTEM_NAME "linux_${ARCHITECTURE}")
  elseif(TARGET_OS STREQUAL "mac")
    set(CPACK_SYSTEM_NAME "macos")
    set(CPACK_GENERATOR DMG)
  endif()
endif()

set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME})
set(CPACK_ARCHIVE_PORTABLE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-src)
set(CPACK_SOURCE_FILES
  CMakeLists.txt
  README.md
  cmake/
  data/
  datasrc/
  ddnet-libs/
  license.txt
  lsan.supp
  memcheck.supp
  other/
  scripts/
  src/
  storage.cfg
  ubsan.supp
  valgrind.supp
)
set(CPACK_SOURCE_IGNORE_FILES
  "\\\\.pyc$"
  "/\\\\.git"
  "/__pycache__/"
)

regex_inverted(CPACK_SOURCE_FILES_INVERTED ${CPACK_SOURCE_FILES})
escape_regex(PROJECT_SOURCE_DIR_ESCAPED ${PROJECT_SOURCE_DIR})

foreach(str ${CPACK_SOURCE_FILES_INVERTED})
  escape_backslashes(STR_ESCAPED "${PROJECT_SOURCE_DIR_ESCAPED}/${str}")
  list(APPEND CPACK_SOURCE_IGNORE_FILES "${STR_ESCAPED}")
endforeach()

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ${PROJECT_NAME})

if(TOOLS)
  set(TARGET_TOOLS
    config_retrieve
    config_store
    demo_extract_chat
    dilate
    map_convert_07
    map_diff
    map_extract
  )
else()
  set(TARGET_TOOLS)
endif()

set(CPACK_TARGETS)
if(TARGET game-client)
  list(APPEND CPACK_TARGETS game-client)
endif()
if(TARGET game-server)
  list(APPEND CPACK_TARGETS game-server)
endif()
list(APPEND CPACK_TARGETS ${TARGET_TOOLS})

if(STEAMAPI_KIND STREQUAL SHARED)
  list(APPEND CPACK_TARGETS ${TARGET_STEAMAPI})
endif()
set(CPACK_DIRS
  data
  ${COPY_DIRS}
)
set(CPACK_FILES
  license.txt
  storage.cfg
  ${COPY_FILES}
)

set(CPACK_GEN_FILES
  ${VULKAN_SHADER_FILE_LIST}
)

if(TARGET_OS STREQUAL "windows")
  list(APPEND CPACK_FILES other/config_directory.bat)
else()
  list(APPEND CPACK_FILES other/config_directory.sh)
endif()

if(NOT DEV)
  include(GNUInstallDirs)
  install(DIRECTORY data DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/ddnet COMPONENT data)
  if(TARGET game-client)
    install(TARGETS game-client DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
  endif()
  if(STEAMAPI_KIND STREQUAL SHARED)
    install(TARGETS ${TARGET_STEAMAPI} DESTINATION ${CMAKE_INSTALL_LIBDIR}/ddnet COMPONENT client)
  endif()
  if(TARGET game-server)
    install(TARGETS game-server DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
  endif()
  if(ANTIBOT)
    install(TARGETS ${TARGET_ANTIBOT} DESTINATION ${CMAKE_INSTALL_LIBDIR}/ddnet COMPONENT server)
  endif()
  install(TARGETS ${TARGETS_TOOLS} DESTINATION ${CMAKE_INSTALL_LIBDIR}/ddnet COMPONENT tools)
  if(TARGET game-client)
    install(FILES other/ddnet.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications COMPONENT client)
  endif()
  foreach(SIZE 16 32 48 256)
    if(TARGET game-client)
      install(FILES other/icons/DDNet_${SIZE}x${SIZE}x32.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${SIZE}x${SIZE}/apps RENAME ddnet.png COMPONENT client)
    endif()
    if(TARGET game-server)
      install(FILES other/icons/DDNet-Server_${SIZE}x${SIZE}x32.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${SIZE}x${SIZE}/apps RENAME ddnet-server.png COMPONENT server)
    endif()
  endforeach()
  foreach(file ${VULKAN_SHADER_FILE_LIST})
    install(FILES ${PROJECT_BINARY_DIR}/${file} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/ddnet/data/shader/vulkan COMPONENT client)
  endforeach()
endif()

if(DEV)
  # Don't generate CPack targets.
else()
  set(EXTRA_ARGS DESTINATION ${CPACK_PACKAGE_FILE_NAME} COMPONENT portable EXCLUDE_FROM_ALL)
  install(TARGETS ${CPACK_TARGETS} ${EXTRA_ARGS})
  install(DIRECTORY ${CPACK_DIRS} ${EXTRA_ARGS})
  set(CPACK_FILES_TMP ${CPACK_FILES} ${CPACK_GEN_FILES})
  install(FILES ${CPACK_FILES_TMP} ${EXTRA_ARGS})
endif()

set(PACKAGE_TARGETS)
if(CLIENT AND DMGBUILD)
  file(MAKE_DIRECTORY bundle/client/)
  file(MAKE_DIRECTORY bundle/server/)
  configure_file(other/bundle/client/Info.plist.in bundle/client/Info.plist)
  configure_file(other/bundle/server/Info.plist.in bundle/server/Info.plist)

  set(DMG_TMPDIR pack_${CPACK_PACKAGE_FILE_NAME}_dmg)
  set(DMG_MKDIRS
    ${CLIENT_EXECUTABLE}.app
    ${CLIENT_EXECUTABLE}.app/Contents
    ${CLIENT_EXECUTABLE}.app/Contents/Frameworks
    ${CLIENT_EXECUTABLE}.app/Contents/MacOS
    ${CLIENT_EXECUTABLE}.app/Contents/Resources
    ${SERVER_EXECUTABLE}.app
    ${SERVER_EXECUTABLE}.app/Contents
    ${SERVER_EXECUTABLE}.app/Contents/MacOS
    ${SERVER_EXECUTABLE}.app/Contents/Resources
    ${SERVER_EXECUTABLE}.app/Contents/Resources/data
    ${SERVER_EXECUTABLE}.app/Contents/Resources/data/mapres
  )
  set(DMG_MKDIR_COMMANDS)
  foreach(dir ${DMG_MKDIRS})
    list(APPEND DMG_MKDIR_COMMANDS COMMAND ${CMAKE_COMMAND} -E make_directory ${DMG_TMPDIR}/${dir})
  endforeach()

  set(TARGET_TOOLS_FILES)
  foreach(target ${TARGET_TOOLS})
    list(APPEND TARGET_TOOLS_FILES $<TARGET_FILE:${target}>)
  endforeach()

  add_custom_command(OUTPUT ${CPACK_PACKAGE_FILE_NAME}.dmg
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${DMG_TMPDIR}
    ${DMG_MKDIR_COMMANDS}

    # CLIENT
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/data ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Resources/data
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/other/icons/${CLIENT_EXECUTABLE}.icns ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Resources/
    COMMAND ${CMAKE_COMMAND} -E copy bundle/client/Info.plist ${PROJECT_SOURCE_DIR}/other/bundle/client/PkgInfo ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:game-client> ${TARGET_TOOLS_FILES} ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/MacOS/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/ddnet-libs/sdl/${LIB_DIR}/SDL2.framework ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/SDL2.framework
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/discord/${LIB_DIR}/discord_game_sdk.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/freetype/${LIB_DIR}/libfreetype.6.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/png/${LIB_DIR}/libpng16.16.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/ffmpeg/${LIB_DIR}/libavcodec.61.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/ffmpeg/${LIB_DIR}/libavformat.61.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/ffmpeg/${LIB_DIR}/libavutil.59.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/ffmpeg/${LIB_DIR}/libswresample.5.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ddnet-libs/ffmpeg/${LIB_DIR}/libswscale.8.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/libsteam_api.dylib ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/Frameworks/
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/darwin_strip_rpath.py ${CMAKE_OTOOL} ${CMAKE_INSTALL_NAME_TOOL} ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/MacOS/${CLIENT_EXECUTABLE}
    COMMAND ${CMAKE_INSTALL_NAME_TOOL} -add_rpath @loader_path/../Frameworks ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/MacOS/${CLIENT_EXECUTABLE}

    # SERVER
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/data/maps ${DMG_TMPDIR}/${SERVER_EXECUTABLE}.app/Contents/Resources/data/maps
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/other/icons/${SERVER_EXECUTABLE}.icns ${DMG_TMPDIR}/${SERVER_EXECUTABLE}.app/Contents/Resources/
    COMMAND ${CMAKE_COMMAND} -E copy bundle/server/Info.plist ${PROJECT_SOURCE_DIR}/other/bundle/server/PkgInfo ${DMG_TMPDIR}/${SERVER_EXECUTABLE}.app/Contents/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:game-server> $<TARGET_FILE:game-server-launcher> ${DMG_TMPDIR}/${SERVER_EXECUTABLE}.app/Contents/MacOS/
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/darwin_strip_rpath.py ${CMAKE_OTOOL} ${CMAKE_INSTALL_NAME_TOOL} ${DMG_TMPDIR}/${SERVER_EXECUTABLE}.app/Contents/MacOS/${SERVER_EXECUTABLE}
    COMMAND ${CMAKE_COMMAND} -E copy ${DMG_TMPDIR}/${SERVER_EXECUTABLE}.app/Contents/MacOS/${SERVER_EXECUTABLE} ${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app/Contents/MacOS/${SERVER_EXECUTABLE}

    # DMG
    COMMAND dmgbuild -s ${PROJECT_SOURCE_DIR}/other/dmgsettings.py -D client=${DMG_TMPDIR}/${CLIENT_EXECUTABLE}.app -D server=${DMG_TMPDIR}/${SERVER_EXECUTABLE}.app -D background=${PROJECT_SOURCE_DIR}/other/dmgbackground.png "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION}" ${CPACK_PACKAGE_FILE_NAME}.dmg

    DEPENDS
      game-client
      ${TARGET_STEAMAPI}
      game-server-launcher
      game-server
      ${PROJECT_BINARY_DIR}/bundle/client/Info.plist
      ${PROJECT_BINARY_DIR}/bundle/server/Info.plist
      data
      other/bundle/client/PkgInfo
      other/bundle/server/PkgInfo
      other/dmgbackground.png
      other/dmgsettings.py
      other/icons/${CLIENT_EXECUTABLE}.icns
      other/icons/${SERVER_EXECUTABLE}.icns
  )
  add_custom_target(package_dmg DEPENDS ${CPACK_PACKAGE_FILE_NAME}.dmg)
  list(APPEND PACKAGE_TARGETS package_dmg)
endif()

foreach(ext zip tar.gz tar.xz)
  set(TAR_MODE c)
  set(TAR_EXTRA_ARGS)
  string(REPLACE . _ EXT_SLUG ${ext})

  set(TMPDIR pack_${CPACK_PACKAGE_FILE_NAME}_${EXT_SLUG}/${CPACK_PACKAGE_FILE_NAME})

  set(COPY_FILE_COMMANDS)
  set(COPY_DIR_COMMANDS)
  set(COPY_TARGET_COMMANDS)
  set(STRIP_TARGET_COMMANDS)
  foreach(file ${CPACK_FILES})
    list(APPEND COPY_FILE_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/${file} ${TMPDIR}/)
  endforeach()
  foreach(file ${CPACK_GEN_FILES})
    list(APPEND COPY_FILE_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/${file} ${TMPDIR}/${file})
  endforeach()
  foreach(dir ${CPACK_DIRS})
    list(APPEND COPY_DIR_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/${dir} ${TMPDIR}/${dir})
  endforeach()
  foreach(target ${CPACK_TARGETS})
    list(APPEND COPY_TARGET_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}> ${TMPDIR}/)
    if(NOT TARGET_OS STREQUAL "mac" AND NOT (TARGET_OS STREQUAL "windows" AND TARGET_CPU_ARCHITECTURE STREQUAL "arm64"))
      list(APPEND STRIP_TARGET_COMMANDS COMMAND strip -s ${TMPDIR}/$<TARGET_FILE_NAME:${target}>)
    endif()
  endforeach()

  if(ext STREQUAL zip)
    set(TAR_EXTRA_ARGS --format=zip)
  elseif(ext STREQUAL tar.gz)
    set(TAR_MODE cz)
  elseif(ext STREQUAL tar.xz)
    set(TAR_MODE cJ)
  endif()
  add_custom_command(OUTPUT ${CPACK_PACKAGE_FILE_NAME}.${ext}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TMPDIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TMPDIR}
    ${COPY_FILE_COMMANDS}
    ${COPY_DIR_COMMANDS}
    ${COPY_TARGET_COMMANDS}
    ${STRIP_TARGET_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E chdir pack_${CPACK_PACKAGE_FILE_NAME}_${EXT_SLUG} ${CMAKE_COMMAND} -E tar ${TAR_MODE} ../${CPACK_PACKAGE_FILE_NAME}.${ext} ${TAR_EXTRA_ARGS} -- ${CPACK_PACKAGE_FILE_NAME}/
    DEPENDS ${CPACK_TARGETS}
  )
  add_custom_target(package_${EXT_SLUG} DEPENDS ${CPACK_PACKAGE_FILE_NAME}.${ext})
  list(APPEND PACKAGE_TARGETS package_${EXT_SLUG})
endforeach()

set(PACKAGE_DEFAULT tar_xz)
if(TARGET_OS STREQUAL "windows")
  set(PACKAGE_DEFAULT zip)
elseif(TARGET_OS STREQUAL "mac")
  set(PACKAGE_DEFAULT dmg)
endif()
add_custom_target(package_default DEPENDS package_${PACKAGE_DEFAULT})

add_custom_target(package_all DEPENDS ${PACKAGE_TARGETS})

# Unset these variables, they might do something in the future of CPack.
unset(CPACK_SOURCE_FILES)
unset(CPACK_SOURCE_FILES_INVERTED)
unset(CPACK_TARGETS)
unset(CPACK_DIRS)
unset(CPACK_FILES)
unset(CPACK_GEN_FILES)

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

if(ANTIBOT)
  # Allow the antibot library to use functions from the server binary.
  add_cxx_compiler_flag_if_supported(OUR_FLAGS_LINK -rdynamic)
  set_own_rpath(game-server)
endif()

set(TARGETS ${TARGETS_OWN} ${TARGETS_DEP})

foreach(target ${TARGETS})
  if(MSVC)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded$<${DBG}:Debug>) # Use static CRT
    target_compile_options(${target} PRIVATE /MP) # Use multiple cores
    target_compile_options(${target} PRIVATE /EHsc) # Only catch C++ exceptions with catch.
    target_compile_options(${target} PRIVATE /GS) # Protect the stack pointer.
    target_compile_options(${target} PRIVATE /wd4996) # Use of non-_s functions.
    target_compile_options(${target} PRIVATE /utf-8) # Use UTF-8 for source files.
  endif()
  if(OUR_FLAGS_LINK)
    target_link_libraries(${target} ${OUR_FLAGS_LINK})
  endif()
  if(OUR_FLAGS)
    target_compile_options(${target} PRIVATE ${OUR_FLAGS})
  endif()
  if(DEFINE_FORTIFY_SOURCE)
    if(MINGW)
      target_compile_definitions(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=0>) # Currently broken in MinGW, see https://sourceforge.net/p/mingw-w64/discussion/723798/thread/b9d24f041f/
    else()
      target_compile_definitions(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>) # Detect some buffer overflows.
    endif()
  endif()
  if(ENABLE_IPO)
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
  if(TARGET_OS STREQUAL "emscripten")
    # Preload data directory into the filesystem
    target_link_options(${target} PRIVATE --preload-file data)
  endif()
endforeach()

foreach(target ${TARGETS_LINK})
  if(MSVC)
    set_property(TARGET ${target} APPEND PROPERTY LINK_FLAGS /SAFESEH:NO) # Disable SafeSEH because the shipped libraries don't support it (would cause error LNK2026 otherwise).
  endif()
  if(TARGET_OS STREQUAL "mac")
    target_link_libraries(${target} -stdlib=libc++)
    target_link_libraries(${target} "-framework SystemConfiguration") # Required by curl 7.79.0
  endif()
  if((MINGW OR TARGET_OS STREQUAL "linux") AND PREFER_BUNDLED_LIBS)
    # Statically link the standard libraries with on MinGW/Linux so we don't
    # have to ship them as DLLs.
    target_link_libraries(${target} -static-libgcc)
    target_link_libraries(${target} -static-libstdc++)
  endif()
endforeach()

foreach(target ${TARGETS_OWN})
  set_property(TARGET ${target} PROPERTY CXX_STANDARD 17)
  set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET ${target} PROPERTY CXX_EXTENSIONS OFF)

  if(MSVC)
    target_compile_options(${target} PRIVATE /wd4244) # Possible loss of data (float -> int, int -> float, etc.).
    target_compile_options(${target} PRIVATE /wd4267) # Possible loss of data (size_t - int on win64).
    target_compile_options(${target} PRIVATE /wd4800) # Implicit conversion of int to bool.
  endif()
  if(TARGET_OS STREQUAL "windows")
    # See https://learn.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers
    target_compile_definitions(${target} PRIVATE NOMINMAX) # windows.h shouldn't define min/max macros
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN) # windows.h shouldn't define the name IStorage
    # 0x0501 (Windows XP) is required for mingw to get getaddrinfo to work
    # 0x0600 (Windows Vista) is required to use RegGetValueW and RegDeleteTreeW
    target_compile_definitions(${target} PRIVATE NTDDI_VERSION=0x06000000) # Minimum OS version (new macro, since Vista)
    target_compile_definitions(${target} PRIVATE _WIN32_WINNT=0x0600) # Minimum OS version (old macro, both must be defined)
    target_compile_definitions(${target} PRIVATE UNICODE) # Windows headers
    target_compile_definitions(${target} PRIVATE _UNICODE) # C-runtime
  endif()
  if(OUR_FLAGS_OWN)
    target_compile_options(${target} PRIVATE ${OUR_FLAGS_OWN})
  endif()
  target_include_directories(${target} PRIVATE ${PROJECT_BINARY_DIR}/src)
  target_include_directories(${target} PRIVATE src)
  target_include_directories(${target} PRIVATE src/rust-bridge)
  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:CONF_DEBUG>)
  target_include_directories(${target} SYSTEM PRIVATE ${CURL_INCLUDE_DIRS} ${SQLite3_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
  target_compile_definitions(${target} PRIVATE GLEW_STATIC)
  target_compile_definitions(${target} PRIVATE _FILE_OFFSET_BITS=64) # Ensure off_t is 64 bit for ftello and fseeko functions
  if(CRYPTO_FOUND)
    target_compile_definitions(${target} PRIVATE CONF_OPENSSL)
    target_include_directories(${target} SYSTEM PRIVATE ${CRYPTO_INCLUDE_DIRS})
  endif()
  if(WEBSOCKETS)
    target_compile_definitions(${target} PRIVATE CONF_WEBSOCKETS)
    target_include_directories(${target} SYSTEM PRIVATE ${WEBSOCKETS_INCLUDE_DIRS})
  endif()
  if(UPNP)
    target_compile_definitions(${target} PRIVATE CONF_UPNP)
    target_include_directories(${target} SYSTEM PRIVATE ${MINIUPNPC_INCLUDE_DIRS})
  endif()
  if(VIDEORECORDER)
    target_compile_definitions(${target} PRIVATE CONF_VIDEORECORDER)
  endif()
  if(ANTIBOT)
    target_compile_definitions(${target} PRIVATE CONF_ANTIBOT)
  endif()
  if(EXCEPTION_HANDLING)
    target_compile_definitions(${target} PRIVATE CONF_CRASHDUMP)
  endif()
  if(HEADLESS_CLIENT)
    target_compile_definitions(${target} PRIVATE CONF_HEADLESS_CLIENT)
  endif()
  if(MYSQL)
    target_compile_definitions(${target} PRIVATE CONF_MYSQL)
    target_include_directories(${target} SYSTEM PRIVATE ${MYSQL_INCLUDE_DIRS})
  endif()
  if(TEST_MYSQL)
    target_compile_definitions(${target} PRIVATE CONF_TEST_MYSQL)
  endif()
  if(AUTOUPDATE AND NOT STEAM)
    target_compile_definitions(${target} PRIVATE CONF_AUTOUPDATE)
  endif()
  if(INFORM_UPDATE AND NOT STEAM)
    target_compile_definitions(${target} PRIVATE CONF_INFORM_UPDATE)
  endif()
  if(STEAM)
    target_compile_definitions(${target} PRIVATE PLATFORM_SUFFIX="-steam")
  endif()
  if(DISCORD)
    target_compile_definitions(${target} PRIVATE CONF_DISCORD)
    if(DISCORD_DYNAMIC)
      target_compile_definitions(${target} PRIVATE CONF_DISCORD_DYNAMIC)
    endif()
  endif()
  if(VERSION)
    target_compile_definitions(${target} PRIVATE GAME_RELEASE_VERSION="${VERSION}")
  endif()
  if(TARGET_OS STREQUAL "android")
    if(ANDROID_PACKAGE_NAME)
      target_compile_definitions(${target} PRIVATE ANDROID_PACKAGE_NAME=${ANDROID_PACKAGE_NAME})
    else()
      message(FATAL_ERROR "ANDROID_PACKAGE_NAME must define the package name when compiling for Android (using underscores instead of dots, e.g. org_example_app)")
    endif()
  endif()
endforeach()

foreach(target ${TARGETS_DEP})
  if(MSVC)
    target_compile_options(${target} PRIVATE /W0)
  endif()
  if(OUR_FLAGS_DEP)
    target_compile_options(${target} PRIVATE ${OUR_FLAGS_DEP})
  endif()
endforeach()
