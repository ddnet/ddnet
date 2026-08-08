if(NOT TARGET_OS STREQUAL "linux"
   AND NOT TARGET_OS STREQUAL "emscripten"
   AND NOT TARGET_OS STREQUAL "android")

	set(EGL_FOUND OFF)
	if(NOT EGL_FIND_QUIETLY)
		message(STATUS "EGL not supported on this platform")
	endif()
	return()
endif()

include(FindPackageHandleStandardArgs)

if(TARGET_OS STREQUAL "windows")

	find_path(EGL_INCLUDE_DIR
		NAMES EGL/egl.h
		PATHS
			${EGL_ROOT}/include
			"$ENV{EGL_ROOT}/include"
			"$ENV{ANGLE_ROOT}/include"
	)

	find_library(EGL_LIBRARY
		NAMES EGL libEGL
		PATHS
			${EGL_ROOT}/lib
			${EGL_ROOT}/lib64
			"$ENV{EGL_ROOT}/lib"
			"$ENV{EGL_ROOT}/lib64"
			"$ENV{ANGLE_ROOT}/lib"
	)

else()

	find_library(EGL_LIBRARY
		NAMES EGL libEGL
		PATHS
			/usr/lib
			/usr/local/lib
			/opt/local/lib
		PATH_SUFFIXES
			x86_64-linux-gnu
			aarch64-linux-gnu
	)

	find_path(EGL_INCLUDE_DIR
		NAMES EGL/egl.h
		PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
	)

endif()

find_package_handle_standard_args(EGL
	DEFAULT_MSG
	EGL_LIBRARY
	EGL_INCLUDE_DIR
)

mark_as_advanced(EGL_LIBRARY EGL_INCLUDE_DIR)

if(EGL_FOUND AND NOT TARGET EGL::EGL)
	add_library(EGL::EGL UNKNOWN IMPORTED)

	set_target_properties(EGL::EGL PROPERTIES
		IMPORTED_LOCATION "${EGL_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${EGL_INCLUDE_DIR}"
	)
endif()
