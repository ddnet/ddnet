# Find glslangValidator
find_program(GLSLANG_VALIDATOR_PROGRAM
  NAMES glslang glslangValidator
)
set(GLSLANG_VALIDATOR_PROGRAM_FOUND TRUE)
if(NOT GLSLANG_VALIDATOR_PROGRAM)
  set(GLSLANG_VALIDATOR_PROGRAM_FOUND FALSE)
  if(TARGET_OS STREQUAL "windows")
    if(${TARGET_CPU_ARCHITECTURE} STREQUAL "x86_64")
      set(GLSLANG_VALIDATOR_PROGRAM "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
    else()
      set(GLSLANG_VALIDATOR_PROGRAM "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
    endif()
  endif()

  if(EXISTS ${GLSLANG_VALIDATOR_PROGRAM})
    set(GLSLANG_VALIDATOR_PROGRAM_FOUND TRUE)
  elseif(TARGET_OS STREQUAL "windows" AND ${TARGET_CPU_ARCHITECTURE} STREQUAL "x86_64")
    set(GLSLANG_VALIDATOR_PROGRAM "${PROJECT_SOURCE_DIR}/ddnet-libs/vulkan/windows/lib64/glslangValidator.exe")
    if(EXISTS ${GLSLANG_VALIDATOR_PROGRAM})
      set(GLSLANG_VALIDATOR_PROGRAM_FOUND TRUE)
    endif()
  endif()

  if(NOT GLSLANG_VALIDATOR_PROGRAM_FOUND)
    message(FATAL_ERROR "glslangValidator binary was not found. Install the Vulkan SDK / packages. Alternatively, set VULKAN=OFF to build without Vulkan.")
  endif()
endif()

# Find spirv-opt
find_program(SPIRV_OPTIMIZER_PROGRAM spirv-opt)
set(SPIRV_OPTIMIZER_PROGRAM_FOUND TRUE)
if(NOT SPIRV_OPTIMIZER_PROGRAM)
  set(SPIRV_OPTIMIZER_PROGRAM_FOUND FALSE)
  if(TARGET_OS STREQUAL "windows")
    if (${TARGET_CPU_ARCHITECTURE} STREQUAL "x86_64")
      set(SPIRV_OPTIMIZER_PROGRAM "$ENV{VULKAN_SDK}/Bin/spirv-opt.exe")
    else()
      set(SPIRV_OPTIMIZER_PROGRAM "$ENV{VULKAN_SDK}/Bin32/spirv-opt.exe")
    endif()
  endif()

  if(EXISTS ${SPIRV_OPTIMIZER_PROGRAM})
    set(SPIRV_OPTIMIZER_PROGRAM_FOUND TRUE)
  endif()

  if(NOT SPIRV_OPTIMIZER_PROGRAM_FOUND)
    message(FATAL_ERROR "spirv-opt binary was not found. Install the Vulkan SDK / packages. Alternatively, set VULKAN=OFF to build without Vulkan.")
  endif()
endif()

# Settings
set(TW_VULKAN_VERSION "vulkan100")

set(VULKAN_SHADER_FILE_LIST "")
set(VULKAN_SHADER_OUTPUT_PATHS "")

function(generate_shader_file FILE_ARGS1 FILE_ARGS2 FILE_NAME FILE_OUTPUT_NAME)
  set(INPUT_PATH "${PROJECT_SOURCE_DIR}/data/shader/vulkan/${FILE_NAME}")
  set(OUTPUT_PATH_RELATIVE "data/shader/vulkan/${FILE_OUTPUT_NAME}")
  set(OUTPUT_PATH "${PROJECT_BINARY_DIR}/${OUTPUT_PATH_RELATIVE}")
  set(TMP_OUTPUT_PATH "${OUTPUT_PATH}.tmp")

  add_custom_command(
    OUTPUT "${OUTPUT_PATH}"
    COMMAND ${GLSLANG_VALIDATOR_PROGRAM} --quiet --client ${TW_VULKAN_VERSION} ${FILE_ARGS1} ${FILE_ARGS2} "${INPUT_PATH}" -o "${TMP_OUTPUT_PATH}"
    COMMAND ${SPIRV_OPTIMIZER_PROGRAM} -O "${TMP_OUTPUT_PATH}" -o "${OUTPUT_PATH}"
    COMMAND ${CMAKE_COMMAND} -E remove "${TMP_OUTPUT_PATH}"
    DEPENDS "${INPUT_PATH}"
    COMMENT "Compiling Vulkan shader ${FILE_OUTPUT_NAME}"
    VERBATIM
  )

  list(APPEND VULKAN_SHADER_FILE_LIST "${OUTPUT_PATH_RELATIVE}")
  set(VULKAN_SHADER_FILE_LIST ${VULKAN_SHADER_FILE_LIST} PARENT_SCOPE)
  list(APPEND VULKAN_SHADER_OUTPUT_PATHS "${OUTPUT_PATH}")
  set(VULKAN_SHADER_OUTPUT_PATHS ${VULKAN_SHADER_OUTPUT_PATHS} PARENT_SCOPE)
endfunction()

# Register all Vulkan shaders to compile

# Primitives
generate_shader_file("" "" "prim.frag" "prim.frag.spv")
generate_shader_file("" "" "prim.vert" "prim.vert.spv")

generate_shader_file("-DTW_TEXTURED" "" "prim.frag" "prim_textured.frag.spv")
generate_shader_file("-DTW_TEXTURED" "" "prim.vert" "prim_textured.vert.spv")

generate_shader_file("" "" "prim3d.frag" "prim3d.frag.spv")
generate_shader_file("" "" "prim3d.vert" "prim3d.vert.spv")

generate_shader_file("-DTW_TEXTURED" "" "prim3d.frag" "prim3d_textured.frag.spv")
generate_shader_file("-DTW_TEXTURED" "" "prim3d.vert" "prim3d_textured.vert.spv")

# Text
generate_shader_file("" "" "text.frag" "text.frag.spv")
generate_shader_file("" "" "text.vert" "text.vert.spv")

# Quad container
generate_shader_file("" "" "primex.frag" "primex.frag.spv")
generate_shader_file("" "" "primex.vert" "primex.vert.spv")

generate_shader_file("" "" "primex.frag" "primex_rotationless.frag.spv")
generate_shader_file("-DTW_ROTATIONLESS" "" "primex.vert" "primex_rotationless.vert.spv")

generate_shader_file("-DTW_TEXTURED" "" "primex.frag" "primex_tex.frag.spv")
generate_shader_file("" "" "primex.vert" "primex_tex.vert.spv")

generate_shader_file("-DTW_TEXTURED" "" "primex.frag" "primex_tex_rotationless.frag.spv")
generate_shader_file("-DTW_ROTATIONLESS" "" "primex.vert" "primex_tex_rotationless.vert.spv")

generate_shader_file("" "" "spritemulti.frag" "spritemulti.frag.spv")
generate_shader_file("" "" "spritemulti.vert" "spritemulti.vert.spv")

generate_shader_file("-DTW_PUSH_CONST" "" "spritemulti.frag" "spritemulti_push.frag.spv")
generate_shader_file("-DTW_PUSH_CONST" "" "spritemulti.vert" "spritemulti_push.vert.spv")

# Tile layer
generate_shader_file("" "" "tile.frag" "tile.frag.spv")
generate_shader_file("" "" "tile.vert" "tile.vert.spv")

generate_shader_file("-DTW_TILE_TEXTURED" "" "tile.frag" "tile_textured.frag.spv")
generate_shader_file("-DTW_TILE_TEXTURED" "" "tile.vert" "tile_textured.vert.spv")

generate_shader_file("" "" "tile_border.frag" "tile_border.frag.spv")
generate_shader_file("" "" "tile_border.vert" "tile_border.vert.spv")

generate_shader_file("" "-DTW_TILE_TEXTURED" "tile_border.frag" "tile_border_textured.frag.spv")
generate_shader_file("" "-DTW_TILE_TEXTURED" "tile_border.vert" "tile_border_textured.vert.spv")

# Quad layer
generate_shader_file("" "" "quad.frag" "quad.frag.spv")
generate_shader_file("" "" "quad.vert" "quad.vert.spv")

generate_shader_file("-DTW_QUAD_GROUPED" "" "quad.frag" "quad_grouped.frag.spv")
generate_shader_file("-DTW_QUAD_GROUPED" "" "quad.vert" "quad_grouped.vert.spv")

generate_shader_file("-DTW_QUAD_TEXTURED" "" "quad.frag" "quad_textured.frag.spv")
generate_shader_file("-DTW_QUAD_TEXTURED" "" "quad.vert" "quad_textured.vert.spv")

generate_shader_file("-DTW_QUAD_TEXTURED" "-DTW_QUAD_GROUPED" "quad.frag" "quad_grouped_textured.frag.spv")
generate_shader_file("-DTW_QUAD_TEXTURED" "-DTW_QUAD_GROUPED" "quad.vert" "quad_grouped_textured.vert.spv")

set(VULKAN_SHADER_FILE_LIST ${VULKAN_SHADER_FILE_LIST} CACHE STRING "Vulkan shader file list" FORCE)
set(VULKAN_SHADER_OUTPUT_PATHS ${VULKAN_SHADER_OUTPUT_PATHS} CACHE STRING "Vulkan shader output list" FORCE)
add_custom_target(build_vulkan_shaders DEPENDS ${VULKAN_SHADER_OUTPUT_PATHS})
