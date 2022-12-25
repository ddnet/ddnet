include(CheckCXXSourceCompiles)
include(CheckLibraryExists)

# Sometimes linking against libatomic is required for atomic ops, if
# the platform doesn't support lock-free atomics.

function(check_working_cxx_atomics varname)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++11")
  check_cxx_source_compiles("
#include <atomic>
#include <cstdint>
std::atomic<int> x1;
std::atomic<short> x2;
std::atomic<char> x3;
std::atomic<uint64_t> x (0);
int main() {
  uint64_t i = x.load(std::memory_order_relaxed);
  (void)i;
  ++x3;
  ++x2;
  return ++x1;
}
" ${varname})
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
endfunction(check_working_cxx_atomics)

if(NOT DEFINED COMPILER_IS_GCC_COMPATIBLE)
  if(MSVC OR APPLE)
    set(COMPILER_IS_GCC_COMPATIBLE OFF)
  else()
    set(COMPILER_IS_GCC_COMPATIBLE ON)
  endif()
endif()

if(NOT COMPILER_IS_GCC_COMPATIBLE)
  set(HAVE_ATOMICS_WITHOUT_LIB ON)
else()
  # Check for 64 bit atomic operations.
  # First check if atomics work without the library.
  check_working_cxx_atomics(HAVE_ATOMICS_WITHOUT_LIB)
  # If not, check if the library exists, and atomics work with it.
  if(NOT HAVE_ATOMICS_WITHOUT_LIB)
    check_library_exists(atomic __atomic_load_8 "" HAVE_LIBATOMICS)
    mark_as_advanced(HAVE_LIBATOMICS)
    if(NOT HAVE_LIBATOMICS)
      message(FATAL_ERROR "Host compiler appears to require libatomic, but cannot find it.")
    endif()
    list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
    check_working_cxx_atomics(HAVE_ATOMICS_WITH_LIB)
    if(NOT HAVE_ATOMICS_WITH_LIB)
      message(FATAL_ERROR "Host compiler must support std::atomic!")
    endif()
    list(APPEND CMAKE_C_STANDARD_LIBRARIES "-latomic")
    list(APPEND CMAKE_CXX_STANDARD_LIBRARIES "-latomic")
  endif()
endif()
