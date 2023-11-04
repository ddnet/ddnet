find_program(GLSLANG_VALIDATOR_PROGRAM
  NAMES glslang glslangValidator
)
find_program(SPIRV_OPTIMIZER_PROGRAM spirv-opt)

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
    message(FATAL_ERROR "glslangValidator binary was not found. Did you install the Vulkan SDK / packages ?")
  endif()
endif()

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
endif()

file(GLOB_RECURSE GLSL_SHADER_FILES
  "data/shader/vulkan/*.frag"
  "data/shader/vulkan/*.vert"
)

set(TMP_SHADER_SHA256_LIST "")
foreach(GLSL_SHADER_FILE ${GLSL_SHADER_FILES})
  file(SHA256 ${GLSL_SHADER_FILE} TMP_FILE_SHA)
  set(TMP_SHADER_SHA256_LIST "${TMP_SHADER_SHA256_LIST}${TMP_FILE_SHA}")
endforeach(GLSL_SHADER_FILE)

string(SHA256 GLSL_SHADER_SHA256 "${TMP_SHADER_SHA256_LIST}")
set(GLSL_SHADER_SHA256 "${GLSL_SHADER_SHA256}@v1")

set(FOUND_MATCHING_SHA256_FILE FALSE)

if("${VULKAN_SHADER_FILE_SHA256}" STREQUAL "${GLSL_SHADER_SHA256}")
  set(FOUND_MATCHING_SHA256_FILE TRUE)
endif()

set(TW_VULKAN_VERSION "vulkan100")

set(GLSLANG_VALIDATOR_COMMAND_LIST)
set(GLSLANG_VALIDATOR_DELETE_LIST)
set(SPIRV_OPTIMIZER_COMMAND_LIST)
function(generate_shader_file FILE_ARGS1 FILE_ARGS2 FILE_NAME FILE_OUTPUT_NAME)
  set(FILE_TMP_NAME_POSTFIX "")
  if(SPIRV_OPTIMIZER_PROGRAM_FOUND)
    set(FILE_TMP_NAME_POSTFIX ".tmp")
  endif()
  list(APPEND GLSLANG_VALIDATOR_COMMAND_LIST COMMAND ${GLSLANG_VALIDATOR_PROGRAM} --client ${TW_VULKAN_VERSION} ${FILE_ARGS1} ${FILE_ARGS2} ${FILE_NAME} -o "${PROJECT_BINARY_DIR}/${FILE_OUTPUT_NAME}${FILE_TMP_NAME_POSTFIX}")
  if(SPIRV_OPTIMIZER_PROGRAM_FOUND)
    list(APPEND SPIRV_OPTIMIZER_COMMAND_LIST COMMAND ${SPIRV_OPTIMIZER_PROGRAM} -O "${PROJECT_BINARY_DIR}/${FILE_OUTPUT_NAME}${FILE_TMP_NAME_POSTFIX}" -o "${PROJECT_BINARY_DIR}/${FILE_OUTPUT_NAME}")
    list(APPEND GLSLANG_VALIDATOR_DELETE_LIST "${PROJECT_BINARY_DIR}/${FILE_OUTPUT_NAME}${FILE_TMP_NAME_POSTFIX}")
  endif()
  file(RELATIVE_PATH TMP_SHADER_FILE_REL "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/${FILE_OUTPUT_NAME}")
  list(APPEND VULKAN_SHADER_FILE_LIST "${FILE_OUTPUT_NAME}")
  set(VULKAN_SHADER_FILE_LIST ${VULKAN_SHADER_FILE_LIST} PARENT_SCOPE)
  set(GLSLANG_VALIDATOR_DELETE_LIST ${GLSLANG_VALIDATOR_DELETE_LIST} PARENT_SCOPE)
  set(SPIRV_OPTIMIZER_COMMAND_LIST ${SPIRV_OPTIMIZER_COMMAND_LIST} PARENT_SCOPE)
  set(GLSLANG_VALIDATOR_COMMAND_LIST ${GLSLANG_VALIDATOR_COMMAND_LIST} PARENT_SCOPE)
endfunction()

if(NOT FOUND_MATCHING_SHA256_FILE)
  message(STATUS "Building vulkan shaders")
  execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/data/shader/vulkan/")

  unset(VULKAN_SHADER_FILE_LIST CACHE)
  
  # primitives
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim.frag" "data/shader/vulkan/prim.frag.spv")
  generate_shader_file("-DTW_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim.frag" "data/shader/vulkan/prim_textured.frag.spv")
  
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim.vert" "data/shader/vulkan/prim.vert.spv")
  generate_shader_file("-DTW_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim.vert" "data/shader/vulkan/prim_textured.vert.spv")
  
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim3d.frag" "data/shader/vulkan/prim3d.frag.spv")
  generate_shader_file("-DTW_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim3d.frag" "data/shader/vulkan/prim3d_textured.frag.spv")
  
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim3d.vert" "data/shader/vulkan/prim3d.vert.spv")
  generate_shader_file("-DTW_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/prim3d.vert" "data/shader/vulkan/prim3d_textured.vert.spv")
  
  # text
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/text.frag" "data/shader/vulkan/text.frag.spv")
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/text.vert" "data/shader/vulkan/text.vert.spv")
  
  # quad container
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.frag" "data/shader/vulkan/primex.frag.spv")
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.vert" "data/shader/vulkan/primex.vert.spv")
  
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.frag" "data/shader/vulkan/primex_rotationless.frag.spv")
  generate_shader_file("-DTW_ROTATIONLESS" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.vert" "data/shader/vulkan/primex_rotationless.vert.spv")
  
  generate_shader_file("-DTW_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.frag" "data/shader/vulkan/primex_tex.frag.spv")
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.vert" "data/shader/vulkan/primex_tex.vert.spv")
  
  generate_shader_file("-DTW_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.frag" "data/shader/vulkan/primex_tex_rotationless.frag.spv")
  generate_shader_file("-DTW_ROTATIONLESS" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/primex.vert" "data/shader/vulkan/primex_tex_rotationless.vert.spv")
  
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/spritemulti.frag" "data/shader/vulkan/spritemulti.frag.spv")
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/spritemulti.vert" "data/shader/vulkan/spritemulti.vert.spv")
  
  generate_shader_file("-DTW_PUSH_CONST" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/spritemulti.frag" "data/shader/vulkan/spritemulti_push.frag.spv")
  generate_shader_file("-DTW_PUSH_CONST" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/spritemulti.vert" "data/shader/vulkan/spritemulti_push.vert.spv")
  
  # tile layer
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile.frag" "data/shader/vulkan/tile.frag.spv")
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile.vert" "data/shader/vulkan/tile.vert.spv")
  
  generate_shader_file("-DTW_TILE_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile.frag" "data/shader/vulkan/tile_textured.frag.spv")
  generate_shader_file("-DTW_TILE_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile.vert" "data/shader/vulkan/tile_textured.vert.spv")
  
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile_border.frag" "data/shader/vulkan/tile_border.frag.spv")
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile_border.vert" "data/shader/vulkan/tile_border.vert.spv")
  
  generate_shader_file("" "-DTW_TILE_TEXTURED" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile_border.frag" "data/shader/vulkan/tile_border_textured.frag.spv")
  generate_shader_file("" "-DTW_TILE_TEXTURED" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/tile_border.vert" "data/shader/vulkan/tile_border_textured.vert.spv")

  # quad layer
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.frag" "data/shader/vulkan/quad.frag.spv")
  generate_shader_file("" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.vert" "data/shader/vulkan/quad.vert.spv")
  
  generate_shader_file("-DTW_PUSH_CONST" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.frag" "data/shader/vulkan/quad_push.frag.spv")
  generate_shader_file("-DTW_PUSH_CONST" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.vert" "data/shader/vulkan/quad_push.vert.spv")
  
  generate_shader_file("-DTW_QUAD_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.frag" "data/shader/vulkan/quad_textured.frag.spv")
  generate_shader_file("-DTW_QUAD_TEXTURED" "" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.vert" "data/shader/vulkan/quad_textured.vert.spv")
  
  generate_shader_file("-DTW_QUAD_TEXTURED" "-DTW_PUSH_CONST" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.frag" "data/shader/vulkan/quad_push_textured.frag.spv")
  generate_shader_file("-DTW_QUAD_TEXTURED" "-DTW_PUSH_CONST" "${PROJECT_SOURCE_DIR}/data/shader/vulkan/quad.vert" "data/shader/vulkan/quad_push_textured.vert.spv")

  execute_process(${GLSLANG_VALIDATOR_COMMAND_LIST} RESULT_VARIABLE STATUS)
  if(STATUS AND NOT STATUS EQUAL 0)
    message(FATAL_ERROR "${GLSLANG_VALIDATOR_COMMAND_LIST} failed")
  endif()
  if(SPIRV_OPTIMIZER_PROGRAM_FOUND)
    execute_process(${SPIRV_OPTIMIZER_COMMAND_LIST} RESULT_VARIABLE STATUS)
    if(STATUS AND NOT STATUS EQUAL 0)
      message(FATAL_ERROR "${SPIRV_OPTIMIZER_COMMAND_LIST} failed")
    endif()
    file(REMOVE ${GLSLANG_VALIDATOR_DELETE_LIST})
  endif()

  set(VULKAN_SHADER_FILE_LIST ${VULKAN_SHADER_FILE_LIST} CACHE STRING "Vulkan shader file list" FORCE)

  message(STATUS "Finished building vulkan shaders")
  set(VULKAN_SHADER_FILE_SHA256 ${GLSL_SHADER_SHA256} CACHE STRING "Vulkan shader file hash" FORCE)
endif()
