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
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(TARGET_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(TARGET_OS "mac")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
  set(TARGET_OS "haiku")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(TARGET_OS "android")
endif()

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.18)
  include(CheckLinkerFlag)
endif()
include(CheckSymbolExists)
if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Emscripten"))
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

option(EXCEPTION_HANDLING "Enable exception handling (only works with Windows as of now)" OFF)
option(IPO "Enable interprocedural optimizations" OFF)
option(FUSE_LD "Linker to use" OFF)
option(SECURITY_COMPILER_FLAGS "Whether to set security-relevant compiler flags like -D_FORTIFY_SOURCE=2 and -fstack-protector-strong" ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
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
    add_cxx_compiler_flag_if_supported(OUR_FLAGS -DCONF_EXCEPTION_HANDLING)
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

if(MSVC)
  if(EXCEPTION_HANDLING)
  add_cxx_compiler_flag_if_supported(OUR_FLAGS /DCONF_EXCEPTION_HANDLING)
  endif()
endif()

if(NOT MSVC AND NOT HAIKU AND SECURITY_COMPILER_FLAGS)
  check_c_compiler_flag("-O2;-Wp,-Werror;-D_FORTIFY_SOURCE=2" DEFINE_FORTIFY_SOURCE) # Some distributions define _FORTIFY_SOURCE by themselves.
endif()

########################################################################
# COMMON FUNCTIONS
########################################################################

function(set_glob VAR GLOBBING EXTS DIRECTORY) # ...
  set(GLOBS)
  foreach(ext ${EXTS})
    list(APPEND GLOBS "${DIRECTORY}/*.${ext}")
  endforeach()
  file(${GLOBBING} GLOB_RESULT ${GLOBS})
  list(SORT GLOB_RESULT)
  set(FILES)
  foreach(file ${ARGN})
    list(APPEND FILES "${PROJECT_SOURCE_DIR}/${DIRECTORY}/${file}")
  endforeach()

  if(NOT FILES STREQUAL GLOB_RESULT)
    message(AUTHOR_WARNING "${VAR} does not contain every file from directory ${DIRECTORY}")
    set(LIST_BUT_NOT_GLOB)
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
    if(NOT LIST_BUT_NOT_GLOB AND NOT GLOB_BUT_NOT_LIST)
      message(AUTHOR_WARNING "${VAR} is not alphabetically sorted")
    endif()
  endif()

  set(${VAR} ${FILES} PARENT_SCOPE)
endfunction()

function(set_src VAR GLOBBING DIRECTORY) # ...
  set_glob(${VAR} ${GLOBBING} "c;cpp;h" ${DIRECTORY} ${ARGN})
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
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
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
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
     set(TMP_TARGET_OS webasm)
    endif()
    set("HINTS_${VARIABLE}_INCLUDEDIR" "ddnet-libs/${NAME}/include" "ddnet-libs/${NAME}/include/${TMP_TARGET_OS}" PARENT_SCOPE)
  endif()
endfunction()

if(CMAKE_CROSSCOMPILING)
  if(TARGET_OS STREQUAL "android" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
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
  find_package(ExceptionHandling)
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
if(TARGET_OS STREQUAL "android" AND CLIENT AND NOT(CRYPTO_FOUND))
  message(SEND_ERROR "You must install OpenSSL to compile the ${CMAKE_PROJECT_NAME} client")
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
    set(PLATFORM_CLIENT_LIBS ${PLATFORM_LIBS})
    set(PLATFORM_CLIENT_INCLUDE_DIRS)
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

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(PLATFORM_CLIENT_LIBS GL)
  set(PLATFORM_CLIENT_INCLUDE_DIRS "")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
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

set_glob(RUST_BASE GLOB_RECURSE "rs;toml" src/base
  Cargo.toml
  color.rs
  lib.rs
  rust.rs
)

set_glob(RUST_ENGINE_INTERFACE GLOB "rs;toml" src/engine
  Cargo.toml
  console.rs
  lib.rs
)

set_glob(RUST_ENGINE_SHARED GLOB_RECURSE "rs;toml" src/engine/shared
  Cargo.toml
  build.rs
  config.rs
  lib.rs
  rust_version.rs
)

set_src(RUST_BRIDGE_SHARED GLOB_RECURSE src/rust-bridge
  cpp/console.cpp
  cpp/console.h
  engine/shared/rust_version.cpp
  engine/shared/rust_version.h
)

set_glob(RUST_MASTERSRV GLOB "rs;toml" src/mastersrv/src
  addr.rs
  locations.rs
  main.rs
)

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

set(EXPECTED_DATA
  arrow.png
  assets/entities/comfort/ddnet.png
  assets/entities/license.txt
  assets/game/game_06.png
  audio/foley_body_impact-01.wv
  audio/foley_body_impact-02.wv
  audio/foley_body_impact-03.wv
  audio/foley_body_splat-01.wv
  audio/foley_body_splat-02.wv
  audio/foley_body_splat-03.wv
  audio/foley_body_splat-04.wv
  audio/foley_dbljump-01.wv
  audio/foley_dbljump-02.wv
  audio/foley_dbljump-03.wv
  audio/foley_foot_left-01.wv
  audio/foley_foot_left-02.wv
  audio/foley_foot_left-03.wv
  audio/foley_foot_left-04.wv
  audio/foley_foot_right-01.wv
  audio/foley_foot_right-02.wv
  audio/foley_foot_right-03.wv
  audio/foley_foot_right-04.wv
  audio/foley_land-01.wv
  audio/foley_land-02.wv
  audio/foley_land-03.wv
  audio/foley_land-04.wv
  audio/hook_attach-01.wv
  audio/hook_attach-02.wv
  audio/hook_attach-03.wv
  audio/hook_loop-01.wv
  audio/hook_loop-02.wv
  audio/hook_noattach-01.wv
  audio/hook_noattach-02.wv
  audio/hook_noattach-03.wv
  audio/music_menu.wv
  audio/sfx_ctf_cap_pl.wv
  audio/sfx_ctf_drop.wv
  audio/sfx_ctf_grab_en.wv
  audio/sfx_ctf_grab_pl.wv
  audio/sfx_ctf_rtn.wv
  audio/sfx_hit_strong-01.wv
  audio/sfx_hit_strong-02.wv
  audio/sfx_hit_weak-01.wv
  audio/sfx_hit_weak-02.wv
  audio/sfx_hit_weak-03.wv
  audio/sfx_msg-client.wv
  audio/sfx_msg-highlight.wv
  audio/sfx_msg-server.wv
  audio/sfx_pickup_arm-01.wv
  audio/sfx_pickup_arm-02.wv
  audio/sfx_pickup_arm-03.wv
  audio/sfx_pickup_arm-04.wv
  audio/sfx_pickup_gun.wv
  audio/sfx_pickup_hrt-01.wv
  audio/sfx_pickup_hrt-02.wv
  audio/sfx_pickup_launcher.wv
  audio/sfx_pickup_ninja.wv
  audio/sfx_pickup_sg.wv
  audio/sfx_skid-01.wv
  audio/sfx_skid-02.wv
  audio/sfx_skid-03.wv
  audio/sfx_skid-04.wv
  audio/sfx_spawn_wpn-01.wv
  audio/sfx_spawn_wpn-02.wv
  audio/sfx_spawn_wpn-03.wv
  audio/vo_teefault_cry-01.wv
  audio/vo_teefault_cry-02.wv
  audio/vo_teefault_ninja-01.wv
  audio/vo_teefault_ninja-02.wv
  audio/vo_teefault_ninja-03.wv
  audio/vo_teefault_ninja-04.wv
  audio/vo_teefault_pain_long-01.wv
  audio/vo_teefault_pain_long-02.wv
  audio/vo_teefault_pain_short-01.wv
  audio/vo_teefault_pain_short-02.wv
  audio/vo_teefault_pain_short-03.wv
  audio/vo_teefault_pain_short-04.wv
  audio/vo_teefault_pain_short-05.wv
  audio/vo_teefault_pain_short-06.wv
  audio/vo_teefault_pain_short-07.wv
  audio/vo_teefault_pain_short-08.wv
  audio/vo_teefault_pain_short-09.wv
  audio/vo_teefault_pain_short-10.wv
  audio/vo_teefault_pain_short-11.wv
  audio/vo_teefault_pain_short-12.wv
  audio/vo_teefault_sledge-01.wv
  audio/vo_teefault_sledge-02.wv
  audio/vo_teefault_sledge-03.wv
  audio/vo_teefault_spawn-01.wv
  audio/vo_teefault_spawn-02.wv
  audio/vo_teefault_spawn-03.wv
  audio/vo_teefault_spawn-04.wv
  audio/vo_teefault_spawn-05.wv
  audio/vo_teefault_spawn-06.wv
  audio/vo_teefault_spawn-07.wv
  audio/wp_flump_explo-01.wv
  audio/wp_flump_explo-02.wv
  audio/wp_flump_explo-03.wv
  audio/wp_flump_launch-01.wv
  audio/wp_flump_launch-02.wv
  audio/wp_flump_launch-03.wv
  audio/wp_gun_fire-01.wv
  audio/wp_gun_fire-02.wv
  audio/wp_gun_fire-03.wv
  audio/wp_hammer_hit-01.wv
  audio/wp_hammer_hit-02.wv
  audio/wp_hammer_hit-03.wv
  audio/wp_hammer_swing-01.wv
  audio/wp_hammer_swing-02.wv
  audio/wp_hammer_swing-03.wv
  audio/wp_laser_bnce-01.wv
  audio/wp_laser_bnce-02.wv
  audio/wp_laser_bnce-03.wv
  audio/wp_laser_fire-01.wv
  audio/wp_laser_fire-02.wv
  audio/wp_laser_fire-03.wv
  audio/wp_ninja_attack-01.wv
  audio/wp_ninja_attack-02.wv
  audio/wp_ninja_attack-03.wv
  audio/wp_ninja_attack-04.wv
  audio/wp_ninja_hit-01.wv
  audio/wp_ninja_hit-02.wv
  audio/wp_ninja_hit-03.wv
  audio/wp_ninja_hit-04.wv
  audio/wp_noammo-01.wv
  audio/wp_noammo-02.wv
  audio/wp_noammo-03.wv
  audio/wp_noammo-04.wv
  audio/wp_noammo-05.wv
  audio/wp_shotty_fire-01.wv
  audio/wp_shotty_fire-02.wv
  audio/wp_shotty_fire-03.wv
  audio/wp_switch-01.wv
  audio/wp_switch-02.wv
  audio/wp_switch-03.wv
  autoexec_server.cfg
  blob.png
  censorlist.txt
  communityicons/none.png
  console.png
  console_bar.png
  countryflags/AD.png
  countryflags/AE.png
  countryflags/AF.png
  countryflags/AG.png
  countryflags/AI.png
  countryflags/AL.png
  countryflags/AM.png
  countryflags/AO.png
  countryflags/AR.png
  countryflags/AS.png
  countryflags/AT.png
  countryflags/AU.png
  countryflags/AW.png
  countryflags/AX.png
  countryflags/AZ.png
  countryflags/BA.png
  countryflags/BB.png
  countryflags/BD.png
  countryflags/BE.png
  countryflags/BF.png
  countryflags/BG.png
  countryflags/BH.png
  countryflags/BI.png
  countryflags/BJ.png
  countryflags/BL.png
  countryflags/BM.png
  countryflags/BN.png
  countryflags/BO.png
  countryflags/BR.png
  countryflags/BS.png
  countryflags/BT.png
  countryflags/BW.png
  countryflags/BY.png
  countryflags/BZ.png
  countryflags/CA.png
  countryflags/CC.png
  countryflags/CD.png
  countryflags/CF.png
  countryflags/CG.png
  countryflags/CH.png
  countryflags/CI.png
  countryflags/CK.png
  countryflags/CL.png
  countryflags/CM.png
  countryflags/CN.png
  countryflags/CO.png
  countryflags/CR.png
  countryflags/CU.png
  countryflags/CV.png
  countryflags/CW.png
  countryflags/CX.png
  countryflags/CY.png
  countryflags/CZ.png
  countryflags/DE.png
  countryflags/DJ.png
  countryflags/DK.png
  countryflags/DM.png
  countryflags/DO.png
  countryflags/DZ.png
  countryflags/EC.png
  countryflags/EE.png
  countryflags/EG.png
  countryflags/EH.png
  countryflags/ER.png
  countryflags/ES-CT.png
  countryflags/ES-GA.png
  countryflags/ES.png
  countryflags/ET.png
  countryflags/EU.png
  countryflags/FI.png
  countryflags/FJ.png
  countryflags/FK.png
  countryflags/FM.png
  countryflags/FO.png
  countryflags/FR.png
  countryflags/GA.png
  countryflags/GB-ENG.png
  countryflags/GB-NIR.png
  countryflags/GB-SCT.png
  countryflags/GB-WLS.png
  countryflags/GB.png
  countryflags/GD.png
  countryflags/GE.png
  countryflags/GF.png
  countryflags/GG.png
  countryflags/GH.png
  countryflags/GI.png
  countryflags/GL.png
  countryflags/GM.png
  countryflags/GN.png
  countryflags/GP.png
  countryflags/GQ.png
  countryflags/GR.png
  countryflags/GS.png
  countryflags/GT.png
  countryflags/GU.png
  countryflags/GW.png
  countryflags/GY.png
  countryflags/HK.png
  countryflags/HN.png
  countryflags/HR.png
  countryflags/HT.png
  countryflags/HU.png
  countryflags/ID.png
  countryflags/IE.png
  countryflags/IL.png
  countryflags/IM.png
  countryflags/IN.png
  countryflags/IO.png
  countryflags/IQ.png
  countryflags/IR.png
  countryflags/IS.png
  countryflags/IT.png
  countryflags/JE.png
  countryflags/JM.png
  countryflags/JO.png
  countryflags/JP.png
  countryflags/KE.png
  countryflags/KG.png
  countryflags/KH.png
  countryflags/KI.png
  countryflags/KM.png
  countryflags/KN.png
  countryflags/KP.png
  countryflags/KR.png
  countryflags/KW.png
  countryflags/KY.png
  countryflags/KZ.png
  countryflags/LA.png
  countryflags/LB.png
  countryflags/LC.png
  countryflags/LI.png
  countryflags/LK.png
  countryflags/LR.png
  countryflags/LS.png
  countryflags/LT.png
  countryflags/LU.png
  countryflags/LV.png
  countryflags/LY.png
  countryflags/MA.png
  countryflags/MC.png
  countryflags/MD.png
  countryflags/ME.png
  countryflags/MF.png
  countryflags/MG.png
  countryflags/MH.png
  countryflags/MK.png
  countryflags/ML.png
  countryflags/MM.png
  countryflags/MN.png
  countryflags/MO.png
  countryflags/MP.png
  countryflags/MQ.png
  countryflags/MR.png
  countryflags/MS.png
  countryflags/MT.png
  countryflags/MU.png
  countryflags/MV.png
  countryflags/MW.png
  countryflags/MX.png
  countryflags/MY.png
  countryflags/MZ.png
  countryflags/NA.png
  countryflags/NC.png
  countryflags/NE.png
  countryflags/NF.png
  countryflags/NG.png
  countryflags/NI.png
  countryflags/NL.png
  countryflags/NO.png
  countryflags/NP.png
  countryflags/NR.png
  countryflags/NU.png
  countryflags/NZ.png
  countryflags/OM.png
  countryflags/PA.png
  countryflags/PE.png
  countryflags/PF.png
  countryflags/PG.png
  countryflags/PH.png
  countryflags/PK.png
  countryflags/PL.png
  countryflags/PM.png
  countryflags/PN.png
  countryflags/PR.png
  countryflags/PS.png
  countryflags/PT.png
  countryflags/PW.png
  countryflags/PY.png
  countryflags/QA.png
  countryflags/RE.png
  countryflags/RO.png
  countryflags/RS.png
  countryflags/RU.png
  countryflags/RW.png
  countryflags/SA.png
  countryflags/SB.png
  countryflags/SC.png
  countryflags/SD.png
  countryflags/SE.png
  countryflags/SG.png
  countryflags/SH.png
  countryflags/SI.png
  countryflags/SK.png
  countryflags/SL.png
  countryflags/SM.png
  countryflags/SN.png
  countryflags/SO.png
  countryflags/SR.png
  countryflags/SS.png
  countryflags/ST.png
  countryflags/SV.png
  countryflags/SX.png
  countryflags/SY.png
  countryflags/SZ.png
  countryflags/TC.png
  countryflags/TD.png
  countryflags/TF.png
  countryflags/TG.png
  countryflags/TH.png
  countryflags/TJ.png
  countryflags/TK.png
  countryflags/TL.png
  countryflags/TM.png
  countryflags/TN.png
  countryflags/TO.png
  countryflags/TR.png
  countryflags/TT.png
  countryflags/TV.png
  countryflags/TW.png
  countryflags/TZ.png
  countryflags/UA.png
  countryflags/UG.png
  countryflags/US.png
  countryflags/UY.png
  countryflags/UZ.png
  countryflags/VA.png
  countryflags/VC.png
  countryflags/VE.png
  countryflags/VG.png
  countryflags/VI.png
  countryflags/VN.png
  countryflags/VU.png
  countryflags/WF.png
  countryflags/WS.png
  countryflags/YE.png
  countryflags/ZA.png
  countryflags/ZM.png
  countryflags/ZW.png
  countryflags/default.png
  countryflags/index.txt
  deadtee.png
  debug_font.png
  editor/audio_source.png
  editor/automap/basic_freeze.rules
  editor/automap/ddmax_freeze.rules
  editor/automap/ddnet_grass.rules
  editor/automap/ddnet_tiles.rules
  editor/automap/ddnet_walls.rules
  editor/automap/desert_main.rules
  editor/automap/fadeout.rules
  editor/automap/generic_clear.rules
  editor/automap/generic_unhookable.rules
  editor/automap/generic_unhookable_0.7.rules
  editor/automap/grass_main.rules
  editor/automap/jungle_main.rules
  editor/automap/jungle_midground.rules
  editor/automap/round_tiles.rules
  editor/automap/water.rules
  editor/automap/winter_main.rules
  editor/background.png
  editor/checker.png
  editor/cursor.png
  editor/cursor_resize.png
  editor/entities/DDNet.png
  editor/entities/F-DDrace.png
  editor/entities/FNG.png
  editor/entities/Race.png
  editor/entities/Vanilla.png
  editor/entities/blockworlds.png
  editor/entities_clear/blockworlds.png
  editor/entities_clear/ddnet.png
  editor/entities_clear/ddrace.png
  editor/entities_clear/f-ddrace.png
  editor/entities_clear/fng.png
  editor/entities_clear/race.png
  editor/entities_clear/vanilla.png
  editor/front.png
  editor/speed_arrow.png
  editor/speed_arrow_array.png
  editor/speedup.png
  editor/switch.png
  editor/tele.png
  editor/tune.png
  emoticons.png
  extras.png
  fonts/DejaVuSans.ttf
  fonts/Font_Awesome_6_Free-Solid-900.otf
  fonts/GlowSansJ-Compressed-Book.otf
  fonts/SourceHanSans.ttc
  fonts/index.json
  game.png
  gui_buttons.png
  gui_cursor.png
  gui_icons.png
  gui_logo.png
  hud.png
  languages/arabic.txt
  languages/azerbaijanese.txt
  languages/belarusian.txt
  languages/bosnian.txt
  languages/brazilian_portuguese.txt
  languages/bulgarian.txt
  languages/catalan.txt
  languages/chuvash.txt
  languages/czech.txt
  languages/danish.txt
  languages/dutch.txt
  languages/esperanto.txt
  languages/estonian.txt
  languages/finnish.txt
  languages/french.txt
  languages/galician.txt
  languages/german.txt
  languages/greek.txt
  languages/hungarian.txt
  languages/index.txt
  languages/italian.txt
  languages/japanese.txt
  languages/korean.txt
  languages/kyrgyz.txt
  languages/license.txt
  languages/norwegian.txt
  languages/persian.txt
  languages/polish.txt
  languages/portuguese.txt
  languages/romanian.txt
  languages/russian.txt
  languages/serbian.txt
  languages/serbian_cyrillic.txt
  languages/simplified_chinese.txt
  languages/slovak.txt
  languages/spanish.txt
  languages/swedish.txt
  languages/traditional_chinese.txt
  languages/turkish.txt
  languages/ukrainian.txt
  mapres/basic_freeze.png
  mapres/bg_cloud1.png
  mapres/bg_cloud2.png
  mapres/bg_cloud3.png
  mapres/ddmax_freeze.png
  mapres/ddnet_grass.png
  mapres/ddnet_start.png
  mapres/ddnet_tiles.png
  mapres/ddnet_walls.png
  mapres/desert_background.png
  mapres/desert_doodads.png
  mapres/desert_main.png
  mapres/desert_mountains.png
  mapres/desert_mountains2.png
  mapres/desert_mountains_new_background.png
  mapres/desert_mountains_new_foreground.png
  mapres/desert_sun.png
  mapres/entities.png
  mapres/fadeout.png
  mapres/font_teeworlds.png
  mapres/font_teeworlds_alt.png
  mapres/generic_clear.png
  mapres/generic_deathtiles.png
  mapres/generic_lamps.png
  mapres/generic_unhookable.png
  mapres/generic_unhookable_0.7.png
  mapres/grass_doodads.png
  mapres/grass_doodads_0.7.png
  mapres/grass_main.png
  mapres/grass_main_0.7.png
  mapres/jungle_background.png
  mapres/jungle_deathtiles.png
  mapres/jungle_doodads.png
  mapres/jungle_main.png
  mapres/jungle_midground.png
  mapres/jungle_unhookables.png
  mapres/light.png
  mapres/mixed_tiles.png
  mapres/moon.png
  mapres/mountains.png
  mapres/round_tiles.png
  mapres/snow.png
  mapres/snow_mountain.png
  mapres/stars.png
  mapres/sun.png
  mapres/water.png
  mapres/winter_doodads.png
  mapres/winter_main.png
  mapres/winter_main_0.7.png
  mapres/winter_mountains.png
  mapres/winter_mountains2.png
  mapres/winter_mountains3.png
  maps/Gold\ Mine.map
  maps/LearnToPlay.map
  maps/Sunny\ Side\ Up.map
  maps/Tsunami.map
  maps/Tutorial.map
  maps/coverage.map
  maps/ctf1.map
  maps/ctf2.map
  maps/ctf3.map
  maps/ctf4.map
  maps/ctf5.map
  maps/ctf6.map
  maps/ctf7.map
  maps/dm1.map
  maps/dm2.map
  maps/dm6.map
  maps/dm7.map
  maps/dm8.map
  maps/dm9.map
  maps/license.txt
  maps7/Gold\ Mine.map
  maps7/LearnToPlay.map
  maps7/Sunny\ Side\ Up.map
  maps7/Tsunami.map
  maps7/Tutorial.map
  maps7/readme.txt
  menuimages/demos.png
  menuimages/editor.png
  menuimages/local_server.png
  menuimages/play_game.png
  menuimages/settings.png
  particles.png
  race_flag.png
  shader/pipeline.frag
  shader/pipeline.vert
  shader/prim.frag
  shader/prim.vert
  shader/primex.frag
  shader/primex.vert
  shader/quad.frag
  shader/quad.vert
  shader/spritemulti.frag
  shader/spritemulti.vert
  shader/text.frag
  shader/text.vert
  shader/tile.frag
  shader/tile.vert
  shader/tile_border.frag
  shader/tile_border.vert
  shader/vulkan/prim.frag
  shader/vulkan/prim.vert
  shader/vulkan/prim3d.frag
  shader/vulkan/prim3d.vert
  shader/vulkan/primex.frag
  shader/vulkan/primex.vert
  shader/vulkan/quad.frag
  shader/vulkan/quad.vert
  shader/vulkan/spritemulti.frag
  shader/vulkan/spritemulti.vert
  shader/vulkan/text.frag
  shader/vulkan/text.vert
  shader/vulkan/tile.frag
  shader/vulkan/tile.vert
  shader/vulkan/tile_border.frag
  shader/vulkan/tile_border.vert
  skins/PaladiN.png
  skins/antiantey.png
  skins/beast.png
  skins/blacktee.png
  skins/bluekitty.png
  skins/bluestripe.png
  skins/bomb.png
  skins/brownbear.png
  skins/cammo.png
  skins/cammostripes.png
  skins/chinese_by_whis.png
  skins/coala.png
  skins/coala_bluekitty.png
  skins/coala_bluestripe.png
  skins/coala_cammo.png
  skins/coala_cammostripes.png
  skins/coala_default.png
  skins/coala_limekitty.png
  skins/coala_pinky.png
  skins/coala_redbopp.png
  skins/coala_redstripe.png
  skins/coala_saddo.png
  skins/coala_toptri.png
  skins/coala_twinbop.png
  skins/coala_twintri.png
  skins/coala_warpaint.png
  skins/coala_x_ninja.png
  skins/default.png
  skins/demonlimekitty.png
  skins/dino.png
  skins/dragon.png
  skins/evil.png
  skins/evilwolfe.png
  skins/ghost.png
  skins/ghostjtj.png
  skins/giraffe.png
  skins/greensward.png
  skins/greyfox.png
  skins/greyfox_2.png
  skins/hammie-chew.png
  skins/hammie-whis.png
  skins/jeet.png
  skins/kintaro_2.png
  skins/kitty_bluestripe.png
  skins/kitty_brownbear.png
  skins/kitty_cammo.png
  skins/kitty_cammostripes.png
  skins/kitty_coala.png
  skins/kitty_default.png
  skins/kitty_pinky.png
  skins/kitty_redbopp.png
  skins/kitty_redstripe.png
  skins/kitty_saddo.png
  skins/kitty_toptri.png
  skins/kitty_twinbop.png
  skins/kitty_twintri.png
  skins/kitty_warpaint.png
  skins/kitty_x_ninja.png
  skins/license.txt
  skins/limekitty.png
  skins/mermydon-coala.png
  skins/mermydon.png
  skins/mouse.png
  skins/musmann.png
  skins/nanami.png
  skins/nanas.png
  skins/nersif.png
  skins/oldman.png
  skins/oldschool.png
  skins/penguin.png
  skins/pinky.png
  skins/random.png
  skins/redbopp.png
  skins/redstripe.png
  skins/saddo.png
  skins/santa_bluekitty.png
  skins/santa_bluestripe.png
  skins/santa_brownbear.png
  skins/santa_cammo.png
  skins/santa_cammostripes.png
  skins/santa_coala.png
  skins/santa_default.png
  skins/santa_limekitty.png
  skins/santa_pinky.png
  skins/santa_redbopp.png
  skins/santa_redstripe.png
  skins/santa_saddo.png
  skins/santa_toptri.png
  skins/santa_twinbop.png
  skins/santa_twintri.png
  skins/santa_warpaint.png
  skins/teerasta.png
  skins/toptri.png
  skins/twinbop.png
  skins/twintri.png
  skins/veteran.png
  skins/voodoo_tee.png
  skins/warpaint.png
  skins/wartee.png
  skins/whis.png
  skins/x_ninja.png
  skins/x_spec.png
  skins7/beaver.json
  skins7/bluekitty.json
  skins7/bluestripe.json
  skins7/body/bat.png
  skins7/body/bear.png
  skins7/body/beaver.png
  skins7/body/dog.png
  skins7/body/force.png
  skins7/body/fox.png
  skins7/body/greensward.png
  skins7/body/hippo.png
  skins7/body/kitty.png
  skins7/body/koala.png
  skins7/body/monkey.png
  skins7/body/mouse.png
  skins7/body/piglet.png
  skins7/body/raccoon.png
  skins7/body/spiky.png
  skins7/body/standard.png
  skins7/body/x_ninja.png
  skins7/bot.png
  skins7/brownbear.json
  skins7/bumbler.json
  skins7/cammo.json
  skins7/cammostripes.json
  skins7/cavebat.json
  skins7/decoration/hair.png
  skins7/decoration/twinbopp.png
  skins7/decoration/twinmello.png
  skins7/decoration/twinpen.png
  skins7/decoration/unibop.png
  skins7/decoration/unimelo.png
  skins7/decoration/unipento.png
  skins7/default.json
  skins7/eyes/colorable.png
  skins7/eyes/negative.png
  skins7/eyes/standard.png
  skins7/eyes/standardreal.png
  skins7/eyes/x_ninja.png
  skins7/feet/standard.png
  skins7/force.json
  skins7/fox.json
  skins7/greensward.json
  skins7/greycoon.json
  skins7/greyfox.json
  skins7/hands/standard.png
  skins7/hippo.json
  skins7/koala.json
  skins7/limedog.json
  skins7/limekitty.json
  skins7/marking/bear.png
  skins7/marking/belly1.png
  skins7/marking/belly2.png
  skins7/marking/blush.png
  skins7/marking/bug.png
  skins7/marking/cammo1.png
  skins7/marking/cammo2.png
  skins7/marking/cammostripes.png
  skins7/marking/coonfluff.png
  skins7/marking/donny.png
  skins7/marking/downdony.png
  skins7/marking/duodonny.png
  skins7/marking/fox.png
  skins7/marking/hipbel.png
  skins7/marking/lowcross.png
  skins7/marking/lowpaint.png
  skins7/marking/marksman.png
  skins7/marking/mice.png
  skins7/marking/mixture1.png
  skins7/marking/mixture2.png
  skins7/marking/monkey.png
  skins7/marking/panda1.png
  skins7/marking/panda2.png
  skins7/marking/purelove.png
  skins7/marking/saddo.png
  skins7/marking/setisu.png
  skins7/marking/sidemarks.png
  skins7/marking/singu.png
  skins7/marking/stripe.png
  skins7/marking/striped.png
  skins7/marking/stripes.png
  skins7/marking/stripes2.png
  skins7/marking/thunder.png
  skins7/marking/tiger1.png
  skins7/marking/tiger2.png
  skins7/marking/toptri.png
  skins7/marking/triangular.png
  skins7/marking/tricircular.png
  skins7/marking/tripledon.png
  skins7/marking/tritri.png
  skins7/marking/twinbelly.png
  skins7/marking/twincross.png
  skins7/marking/twintri.png
  skins7/marking/uppy.png
  skins7/marking/warpaint.png
  skins7/marking/warstripes.png
  skins7/marking/whisker.png
  skins7/marking/wildpaint.png
  skins7/marking/wildpatch.png
  skins7/marking/yinyang.png
  skins7/monkey.json
  skins7/paintgre.json
  skins7/pandabear.json
  skins7/panther.json
  skins7/pento.json
  skins7/piggy.json
  skins7/pinky.json
  skins7/raccoon.json
  skins7/redbopp.json
  skins7/redstripe.json
  skins7/saddo.json
  skins7/setisu.json
  skins7/snowti.json
  skins7/spiky.json
  skins7/swardy.json
  skins7/tiger.json
  skins7/tooxy.json
  skins7/toptri.json
  skins7/twinbop.json
  skins7/twintri.json
  skins7/warmouse.json
  skins7/warpaint.json
  skins7/x_ninja.json
  skins7/xmas_hat.png
  strong_weak.png
  themes/auto.png
  themes/autumn.png
  themes/autumn_day.map
  themes/autumn_night.map
  themes/heavens.png
  themes/heavens_day.map
  themes/heavens_night.map
  themes/jungle.png
  themes/jungle_day.map
  themes/jungle_night.map
  themes/newyear.map
  themes/newyear.png
  themes/none.png
  themes/rand.png
  themes/winter.png
  themes/winter_day.map
  themes/winter_night.map
  wordlist.txt
)

set_glob(DATA GLOB_RECURSE "cfg;frag;json;map;otf;png;rules;ttc;ttf;txt;vert;wv" data ${EXPECTED_DATA})

########################################################################
# COPY DATA AND SHARED LIBS
########################################################################

foreach(datafile ${DATA})
  file(RELATIVE_PATH OUT ${PROJECT_SOURCE_DIR}/data ${datafile})
  get_filename_component(DESTINATION data/${OUT} PATH)
  file(MAKE_DIRECTORY ${DESTINATION})
  file(COPY ${datafile} DESTINATION ${DESTINATION})
endforeach()
set(INTEGRATION_TEST_COPY_FILES
  lsan.supp
  memcheck.supp
  ubsan.supp
  valgrind.supp
  scripts/integration_test.sh
)
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
  ${EXCEPTION_HANDLING_COPY_FILES}
  ${SSP_COPY_FILES}
)
file(COPY ${COPY_FILES} DESTINATION .)
# Don't add integration test files to COPY_FILES since we don't want to add
# them in cpack.
file(COPY ${INTEGRATION_TEST_COPY_FILES} DESTINATION .)

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
set_src(BASE GLOB_RECURSE src/base
  bezier.cpp
  bezier.h
  color.cpp
  color.h
  detect.h
  dynamic.h
  hash.cpp
  hash.h
  hash_bundled.cpp
  hash_ctxt.h
  hash_libtomcrypt.cpp
  hash_openssl.cpp
  lock.h
  log.cpp
  log.h
  logger.h
  math.h
  rust.h
  system.cpp
  system.h
  tl/threading.h
  types.h
  unicode/confusables.cpp
  unicode/confusables.h
  unicode/confusables_data.h
  unicode/tolower.cpp
  unicode/tolower.h
  unicode/tolower_data.h
  vmath.h
)
set_src(ENGINE_INTERFACE GLOB src/engine
  antibot.h
  client.h
  config.h
  console.h
  demo.h
  discord.h
  editor.h
  engine.h
  favorites.h
  friends.h
  ghost.h
  graphics.h
  http.h
  image.h
  input.h
  kernel.h
  keys.h
  map.h
  message.h
  notifications.h
  rust.h
  server.h
  serverbrowser.h
  sound.h
  sqlite.h
  steam.h
  storage.h
  textrender.h
  updater.h
  uuid.h
  warning.h
)
set_src(ENGINE_SHARED GLOB_RECURSE src/engine/shared
  assertion_logger.cpp
  assertion_logger.h
  compression.cpp
  compression.h
  config.cpp
  config.h
  config_variables.h
  console.cpp
  console.h
  csv.cpp
  csv.h
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
  host_lookup.cpp
  host_lookup.h
  http.cpp
  http.h
  huffman.cpp
  huffman.h
  jobs.cpp
  jobs.h
  json.cpp
  json.h
  jsonwriter.cpp
  jsonwriter.h
  kernel.cpp
  linereader.cpp
  linereader.h
  localization.h
  map.cpp
  map.h
  masterserver.cpp
  masterserver.h
  memheap.cpp
  memheap.h
  netban.cpp
  netban.h
  network.cpp
  network.h
  network_client.cpp
  network_conn.cpp
  network_console.cpp
  network_console_conn.cpp
  network_server.cpp
  network_stun.cpp
  packer.cpp
  packer.h
  protocol.h
  protocol7.h
  protocol_ex.cpp
  protocol_ex.h
  protocol_ex_msgs.h
  protocolglue.cpp
  protocolglue.h
  ringbuffer.cpp
  ringbuffer.h
  serverinfo.cpp
  serverinfo.h
  sixup_translate_snapshot.cpp
  snapshot.cpp
  snapshot.h
  storage.cpp
  stun.cpp
  stun.h
  teehistorian_ex.cpp
  teehistorian_ex.h
  teehistorian_ex_chunks.h
  translation_context.cpp
  translation_context.h
  uuid_manager.cpp
  uuid_manager.h
  video.cpp
  video.h
  websockets.cpp
  websockets.h
)
set_src(ENGINE_GFX GLOB src/engine/gfx
  image.cpp
  image_loader.cpp
  image_loader.h
  image_manipulation.cpp
  image_manipulation.h
)
set_src(GAME_SHARED GLOB src/game
  alloc.h
  collision.cpp
  collision.h
  gamecore.cpp
  gamecore.h
  layers.cpp
  layers.h
  localization.cpp
  localization.h
  mapbugs.cpp
  mapbugs.h
  mapbugs_list.h
  mapitems.cpp
  mapitems.h
  mapitems_ex.cpp
  mapitems_ex.h
  mapitems_ex_types.h
  prng.cpp
  prng.h
  teamscore.cpp
  teamscore.h
  tuning.h
  version.h
  voting.h
)
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
  set_src(STEAMAPI_SRC GLOB_RECURSE src/steam
    steam_api_flat.h
    steam_api_stub.cpp
  )

  if(STEAM OR TARGET_OS STREQUAL "windows" OR TARGET_OS STREQUAL "mac")
    set(STEAMAPI_KIND SHARED)
  else()
    set(STEAMAPI_KIND STATIC)
  endif()
  set(TARGET_STEAMAPI steam_api)
  add_library(${TARGET_STEAMAPI} ${STEAMAPI_KIND} ${STEAMAPI_SRC})
  list(APPEND TARGETS_OWN ${TARGET_STEAMAPI})

  set_src(ENGINE_CLIENT GLOB_RECURSE src/engine/client
    backend/backend_base.cpp
    backend/backend_base.h
    backend/glsl_shader_compiler.cpp
    backend/glsl_shader_compiler.h
    backend/null/backend_null.cpp
    backend/null/backend_null.h
    backend/opengl/backend_opengl.cpp
    backend/opengl/backend_opengl.h
    backend/opengl/backend_opengl3.cpp
    backend/opengl/backend_opengl3.h
    backend/opengl/opengl_sl.cpp
    backend/opengl/opengl_sl.h
    backend/opengl/opengl_sl_program.cpp
    backend/opengl/opengl_sl_program.h
    backend/opengles/backend_opengles.cpp
    backend/opengles/backend_opengles3.cpp
    backend/opengles/backend_opengles3.h
    backend/opengles/gles_class_defines.h
    backend/opengles/opengles_sl.cpp
    backend/opengles/opengles_sl_program.cpp
    backend/vulkan/backend_vulkan.cpp
    backend/vulkan/backend_vulkan.h
    backend_sdl.cpp
    backend_sdl.h
    blocklist_driver.cpp
    blocklist_driver.h
    checksum.h
    client.cpp
    client.h
    demoedit.cpp
    demoedit.h
    discord.cpp
    enums.h
    favorites.cpp
    friends.cpp
    friends.h
    ghost.cpp
    ghost.h
    graph.cpp
    graph.h
    graphics_defines.h
    graphics_threaded.cpp
    graphics_threaded.h
    input.cpp
    input.h
    keynames.h
    notifications.cpp
    notifications.h
    serverbrowser.cpp
    serverbrowser.h
    serverbrowser_http.cpp
    serverbrowser_http.h
    serverbrowser_ping_cache.cpp
    serverbrowser_ping_cache.h
    sixup_translate_system.cpp
    smooth_time.cpp
    smooth_time.h
    sound.cpp
    sound.h
    sqlite.cpp
    steam.cpp
    text.cpp
    updater.cpp
    updater.h
    video.cpp
    video.h
    warning.cpp
  )
  set_src(GAME_CLIENT GLOB_RECURSE src/game/client
    animstate.cpp
    animstate.h
    component.cpp
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
    components/freezebars.cpp
    components/freezebars.h
    components/ghost.cpp
    components/ghost.h
    components/hud.cpp
    components/hud.h
    components/infomessages.cpp
    components/infomessages.h
    components/items.cpp
    components/items.h
    components/mapimages.cpp
    components/mapimages.h
    components/maplayers.cpp
    components/maplayers.h
    components/mapsounds.cpp
    components/mapsounds.h
    components/menu_background.cpp
    components/menu_background.h
    components/menus.cpp
    components/menus.h
    components/menus_browser.cpp
    components/menus_demo.cpp
    components/menus_ingame.cpp
    components/menus_settings.cpp
    components/menus_settings7.cpp
    components/menus_settings_assets.cpp
    components/menus_start.cpp
    components/motd.cpp
    components/motd.h
    components/nameplates.cpp
    components/nameplates.h
    components/particles.cpp
    components/particles.h
    components/players.cpp
    components/players.h
    components/race_demo.cpp
    components/race_demo.h
    components/scoreboard.cpp
    components/scoreboard.h
    components/skins.cpp
    components/skins.h
    components/skins7.cpp
    components/skins7.h
    components/sounds.cpp
    components/sounds.h
    components/spectator.cpp
    components/spectator.h
    components/statboard.cpp
    components/statboard.h
    components/tooltips.cpp
    components/tooltips.h
    components/voting.cpp
    components/voting.h
    gameclient.cpp
    gameclient.h
    laser_data.cpp
    laser_data.h
    lineinput.cpp
    lineinput.h
    pickup_data.cpp
    pickup_data.h
    prediction/entities/character.cpp
    prediction/entities/character.h
    prediction/entities/dragger.cpp
    prediction/entities/dragger.h
    prediction/entities/laser.cpp
    prediction/entities/laser.h
    prediction/entities/pickup.cpp
    prediction/entities/pickup.h
    prediction/entities/projectile.cpp
    prediction/entities/projectile.h
    prediction/entity.cpp
    prediction/entity.h
    prediction/gameworld.cpp
    prediction/gameworld.h
    projectile_data.cpp
    projectile_data.h
    race.cpp
    race.h
    render.cpp
    render.h
    render_map.cpp
    sixup_translate_game.cpp
    sixup_translate_snapshot.cpp
    skin.h
    ui.cpp
    ui.h
    ui_listbox.cpp
    ui_listbox.h
    ui_rect.cpp
    ui_rect.h
    ui_scrollregion.cpp
    ui_scrollregion.h
  )
  set_src(GAME_EDITOR GLOB_RECURSE src/game/editor
    auto_map.cpp
    auto_map.h
    component.cpp
    component.h
    editor.cpp
    editor.h
    editor_action.h
    editor_actions.cpp
    editor_actions.h
    editor_history.cpp
    editor_history.h
    editor_object.cpp
    editor_object.h
    editor_props.cpp
    editor_server_settings.cpp
    editor_server_settings.h
    editor_trackers.cpp
    editor_trackers.h
    editor_ui.h
    enums.h
    explanations.cpp
    layer_selector.cpp
    layer_selector.h
    map_grid.cpp
    map_grid.h
    map_view.cpp
    map_view.h
    mapitems.h
    mapitems/envelope.cpp
    mapitems/envelope.h
    mapitems/image.cpp
    mapitems/image.h
    mapitems/layer.h
    mapitems/layer_front.cpp
    mapitems/layer_front.h
    mapitems/layer_game.cpp
    mapitems/layer_game.h
    mapitems/layer_group.cpp
    mapitems/layer_group.h
    mapitems/layer_quads.cpp
    mapitems/layer_quads.h
    mapitems/layer_sounds.cpp
    mapitems/layer_sounds.h
    mapitems/layer_speedup.cpp
    mapitems/layer_speedup.h
    mapitems/layer_switch.cpp
    mapitems/layer_switch.h
    mapitems/layer_tele.cpp
    mapitems/layer_tele.h
    mapitems/layer_tiles.cpp
    mapitems/layer_tiles.h
    mapitems/layer_tune.cpp
    mapitems/layer_tune.h
    mapitems/map.cpp
    mapitems/map_io.cpp
    mapitems/sound.cpp
    mapitems/sound.h
    popups.cpp
    prompt.cpp
    prompt.h
    proof_mode.cpp
    proof_mode.h
    quick_action.h
    quick_actions.cpp
    quick_actions.h
    smooth_value.cpp
    smooth_value.h
    tileart.cpp
  )
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
    configure_file("other/manifest/client.manifest.in" "client.manifest")
    set(CLIENT_ICON "other/icons/DDNet.rc")
    if(NOT MINGW)
      set(CLIENT_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/client.manifest")
    else()
      set(CLIENT_MANIFEST "other/manifest/client.rc")
      set_target_properties(${TARGET_STEAMAPI} PROPERTIES PREFIX "")
    endif()
  else()
    set(CLIENT_ICON)
    set(CLIENT_MANIFEST)
  endif()

  # Target
  if(TARGET_OS STREQUAL "android")
    add_library(game-client SHARED
      ${CLIENT_SRC}
      ${CLIENT_ICON}
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
  set_src(ANTIBOT_SRC GLOB src/antibot
    antibot_data.h
    antibot_interface.h
    antibot_null.cpp
  )

  set_src(ENGINE_SERVER GLOB_RECURSE src/engine/server
    antibot.cpp
    antibot.h
    authmanager.cpp
    authmanager.h
    databases/connection.cpp
    databases/connection.h
    databases/connection_pool.cpp
    databases/connection_pool.h
    databases/mysql.cpp
    databases/sqlite.cpp
    main.cpp
    name_ban.cpp
    name_ban.h
    register.cpp
    register.h
    server.cpp
    server.h
    server_logger.cpp
    server_logger.h
    snap_id_pool.cpp
    snap_id_pool.h
    sql_string_helpers.cpp
    sql_string_helpers.h
    upnp.cpp
    upnp.h
  )
  set_src(GAME_SERVER GLOB_RECURSE src/game/server
    ddracechat.cpp
    ddracecommands.cpp
    entities/character.cpp
    entities/character.h
    entities/door.cpp
    entities/door.h
    entities/dragger.cpp
    entities/dragger.h
    entities/dragger_beam.cpp
    entities/dragger_beam.h
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
    gamemodes/mod.cpp
    gamemodes/mod.h
    gameworld.cpp
    gameworld.h
    player.cpp
    player.h
    save.cpp
    save.h
    score.cpp
    score.h
    scoreworker.cpp
    scoreworker.h
    teams.cpp
    teams.h
    teehistorian.cpp
    teehistorian.h
    teeinfo.cpp
    teeinfo.h
  )
  set(GAME_GENERATED_SERVER
    "src/game/generated/server_data.cpp"
    "src/game/generated/server_data.h"
    "src/game/generated/wordlist.h"
  )
  set(SERVER_SRC ${ENGINE_SERVER} ${GAME_SERVER} ${GAME_GENERATED_SERVER})
  if(TARGET_OS STREQUAL "windows")
    set(SERVER_ICON "other/icons/DDNet-Server.rc")
  else()
    set(SERVER_ICON)
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

  # Target
  add_executable(game-server
    ${DEPS}
    ${SERVER_SRC}
    ${SERVER_ICON}
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
    $<TARGET_OBJECTS:rust-bridge-shared>
  )
  set_property(TARGET game-server
      PROPERTY OUTPUT_NAME ${SERVER_EXECUTABLE}
  )
  target_link_libraries(game-server ${LIBS_SERVER})
  target_include_directories(game-server PRIVATE ${PNG_INCLUDE_DIRS})
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
  set_src(TOOLS_SRC GLOB src/tools
    config_common.h
    config_retrieve.cpp
    config_store.cpp
    crapnet.cpp
    demo_extract_chat.cpp
    dilate.cpp
    dummy_map.cpp
    map_convert_07.cpp
    map_create_pixelart.cpp
    map_diff.cpp
    map_extract.cpp
    map_find_env.cpp
    map_optimize.cpp
    map_replace_area.cpp
    map_replace_image.cpp
    map_resave.cpp
    packetgen.cpp
    stun.cpp
    twping.cpp
    unicode_confusables.cpp
    uuid.cpp
  )
  foreach(ABS_T ${TOOLS_SRC})
    file(RELATIVE_PATH T "${PROJECT_SOURCE_DIR}/src/tools/" ${ABS_T})
    if(T MATCHES "\\.cpp$")
      string(REGEX REPLACE "\\.cpp$" "" TOOL "${T}")
      set(TOOL_DEPS ${DEPS})
      set(TOOL_LIBS ${LIBS})
      unset(EXTRA_TOOL_SRC)
      if(TOOL MATCHES "^(dilate|map_convert_07|map_create_pixelart|map_optimize|map_extract|map_replace_image)$")
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

if(GTEST_FOUND OR DOWNLOAD_GTEST)
  set_src(TESTS GLOB src/test
    aio.cpp
    bezier.cpp
    blocklist_driver.cpp
    bytes_be.cpp
    color.cpp
    compression.cpp
    csv.cpp
    datafile.cpp
    editor.cpp
    fs.cpp
    git_revision.cpp
    hash.cpp
    huffman.cpp
    io.cpp
    jobs.cpp
    json.cpp
    jsonwriter.cpp
    linereader.cpp
    mapbugs.cpp
    math.cpp
    memory.cpp
    name_ban.cpp
    net.cpp
    netaddr.cpp
    os.cpp
    packer.cpp
    prng.cpp
    score.cpp
    secure_random.cpp
    serverbrowser.cpp
    serverinfo.cpp
    snapshot.cpp
    str.cpp
    strip_path_and_extension.cpp
    swap_endian.cpp
    teehistorian.cpp
    test.cpp
    test.h
    thread.cpp
    timestamp.cpp
    unix.cpp
    uuid.cpp
  )
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
    src/engine/server/databases/connection.cpp
    src/engine/server/databases/connection.h
    src/engine/server/databases/sqlite.cpp
    src/engine/server/databases/mysql.cpp
    src/engine/server/name_ban.cpp
    src/engine/server/name_ban.h
    src/engine/server/sql_string_helpers.cpp
    src/engine/server/sql_string_helpers.h
    src/game/server/teehistorian.cpp
    src/game/server/teehistorian.h
    src/game/server/scoreworker.cpp
    src/game/server/scoreworker.h
  )

  set(TARGET_TESTRUNNER testrunner)
  add_executable(${TARGET_TESTRUNNER} EXCLUDE_FROM_ALL
    ${TESTS}
    ${TESTS_EXTRA}
    $<TARGET_OBJECTS:engine-gfx>
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
    ${DEPS}
  )
  target_link_libraries(${TARGET_TESTRUNNER} ${MYSQL_LIBRARIES} ${PNG_LIBRARIES} ${GTEST_LIBRARIES} ${LIBS})
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

add_custom_target(run_integration_tests
  COMMAND ${PROJECT_BINARY_DIR}/integration_test.sh ${INTEGRATIONTESTRUNNER_ARGS}
  COMMENT Running integration tests
  DEPENDS game-client game-server
  USES_TERMINAL
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
    map_create_pixelart
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
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
   target_compile_definitions(${target} PRIVATE CONF_WEBASM)
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
