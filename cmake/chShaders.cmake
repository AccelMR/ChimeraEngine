# Compiles HLSL shaders with DXC into ${CH_OUTPUT_DIR}/Shaders, one folder per format:
# SPIRV for Vulkan on every platform and DXIL for Direct3D 12 on Windows.
#
# Each .hlsl file is one shader; its entry points are found by name (VSMain, PSMain,
# CSMain) and each one becomes <name>.<vs|ps|cs>.<spv|dxil>. A line "CH_VARIANT <suffix>
# <DEFINE>" in the file (in a comment) also builds every entry point with <DEFINE> defined
# into <name>_<suffix>.<stage>.<format>, so a shader can have versions without copies of
# its source. A change to any .hlsli in the same folder recompiles every shader of that
# folder.
#
# Engine plugins compile their own shaders with ch_add_plugin_shaders: they go next to the
# plugin, in Plugins/Shaders/<plugin>/<format>, and may include the engine's .hlsli files.

find_program(CH_DXC_EXECUTABLE dxc
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
  DOC "DirectX Shader Compiler, shipped with the Vulkan SDK")
if(NOT CH_DXC_EXECUTABLE)
  message(FATAL_ERROR "dxc not found. Install the Vulkan SDK and set VULKAN_SDK.")
endif()

set(CH_SHADER_MODEL "6_6")
set(CH_SHADER_OUTPUT_DIR "${CH_OUTPUT_DIR}/Shaders")
set(CH_CORE_SHADER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../chCore/Content/Shaders")
cmake_path(NORMAL_PATH CH_CORE_SHADER_SOURCE_DIR)

# ch_add_shaders(<target> <source folder> [OUTPUT_DIR <folder>] [INCLUDE_DIRS <folders>...])
# Creates <target>, which builds every shader of <source folder> into OUTPUT_DIR
# (CH_SHADER_OUTPUT_DIR by default). The .hlsli files of INCLUDE_DIRS can be included too,
# and a change to one of them recompiles every shader of the target.
function(ch_add_shaders TARGET_NAME SOURCE_DIR)
  cmake_parse_arguments(ARG "" "OUTPUT_DIR" "INCLUDE_DIRS" ${ARGN})
  set(OUTPUT_ROOT "${CH_SHADER_OUTPUT_DIR}")
  if(ARG_OUTPUT_DIR)
    set(OUTPUT_ROOT "${ARG_OUTPUT_DIR}")
  endif()

  file(GLOB HLSL_FILES CONFIGURE_DEPENDS "${SOURCE_DIR}/*.hlsl")
  file(GLOB HLSL_INCLUDES CONFIGURE_DEPENDS "${SOURCE_DIR}/*.hlsli")
  set(INCLUDE_FLAGS -I "${SOURCE_DIR}")
  set(EXTRA_INCLUDES "")
  foreach(INCLUDE_DIR ${ARG_INCLUDE_DIRS})
    file(GLOB DIR_INCLUDES CONFIGURE_DEPENDS "${INCLUDE_DIR}/*.hlsli")
    list(APPEND EXTRA_INCLUDES ${DIR_INCLUDES})
    list(APPEND INCLUDE_FLAGS -I "${INCLUDE_DIR}")
  endforeach()

  set(FORMATS SPIRV)
  if(WIN32)
    list(APPEND FORMATS DXIL)
  endif()

  set(OUTPUTS "")
  foreach(HLSL_FILE ${HLSL_FILES})
    get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
    file(READ ${HLSL_FILE} HLSL_TEXT)

    # "base" is the shader as written; each CH_VARIANT line adds one built with its define.
    set(VARIANTS base)
    string(REGEX MATCHALL "CH_VARIANT[ \t]+[A-Za-z0-9_]+[ \t]+[A-Za-z0-9_]+"
           VARIANT_LINES "${HLSL_TEXT}")
    foreach(VARIANT_LINE ${VARIANT_LINES})
      string(REGEX REPLACE "CH_VARIANT[ \t]+([A-Za-z0-9_]+)[ \t]+([A-Za-z0-9_]+)" "\\1:\\2"
             VARIANT "${VARIANT_LINE}")
      list(APPEND VARIANTS ${VARIANT})
    endforeach()

    foreach(VARIANT ${VARIANTS})
      if(VARIANT STREQUAL "base")
        set(OUTPUT_NAME ${SHADER_NAME})
        set(DEFINE_FLAGS "")
      else()
        string(REPLACE ":" ";" VARIANT_PARTS ${VARIANT})
        list(GET VARIANT_PARTS 0 VARIANT_SUFFIX)
        list(GET VARIANT_PARTS 1 VARIANT_DEFINE)
        set(OUTPUT_NAME ${SHADER_NAME}_${VARIANT_SUFFIX})
        set(DEFINE_FLAGS -D ${VARIANT_DEFINE})
      endif()

      foreach(STAGE vs ps cs)
        string(TOUPPER ${STAGE} STAGE_UPPER)
        set(ENTRY_POINT "${STAGE_UPPER}Main")
        if(NOT HLSL_TEXT MATCHES "(^|[^A-Za-z0-9_])${ENTRY_POINT}[ \t]*\\(")
          continue()
        endif()

        foreach(FORMAT ${FORMATS})
          if(FORMAT STREQUAL "SPIRV")
            set(EXTENSION spv)
            # Storage images get no format from their HLSL type, so one RWTexture type can
            # write any format (needs the WithoutFormat device features).
            set(FORMAT_FLAGS -spirv -fspv-target-env=vulkan1.3 -fspv-use-unknown-image-format)
          else()
            set(EXTENSION dxil)
            # DXIL has no use for the [[vk::...]] attributes, so their warnings are noise.
            set(FORMAT_FLAGS -Wno-ignored-attributes)
          endif()

          # CH_OUTPUT_DIR holds $<CONFIG> with Visual Studio; OUTPUT accepts it since
          # CMake 3.20.
          set(OUTPUT_FILE
              "${OUTPUT_ROOT}/${FORMAT}/${OUTPUT_NAME}.${STAGE}.${EXTENSION}")
          add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_ROOT}/${FORMAT}"
            COMMAND ${CH_DXC_EXECUTABLE}
                    -T ${STAGE}_${CH_SHADER_MODEL} -E ${ENTRY_POINT} -HV 2021
                    ${FORMAT_FLAGS} ${DEFINE_FLAGS}
                    "$<IF:$<CONFIG:Debug>,-Od;-Zi;-Qembed_debug,-O3>"
                    ${INCLUDE_FLAGS}
                    -Fo ${OUTPUT_FILE} ${HLSL_FILE}
            DEPENDS ${HLSL_FILE} ${HLSL_INCLUDES} ${EXTRA_INCLUDES}
            COMMENT "Compiling ${OUTPUT_NAME}.${STAGE} (${FORMAT})"
            COMMAND_EXPAND_LISTS
            VERBATIM)
          list(APPEND OUTPUTS ${OUTPUT_FILE})
        endforeach()
      endforeach()
    endforeach()
  endforeach()

  # Listed only so they show up in the IDE; Visual Studio would build them with FXC otherwise.
  set_source_files_properties(${HLSL_FILES} ${HLSL_INCLUDES} PROPERTIES VS_TOOL_OVERRIDE None)
  add_custom_target(${TARGET_NAME} ALL
    DEPENDS ${OUTPUTS}
    SOURCES ${HLSL_FILES} ${HLSL_INCLUDES})
endfunction()

# ch_add_plugin_shaders(<plugin target>)
# Builds the shaders of the plugin's Shaders folder into Plugins/Shaders/<plugin>, with the
# engine's shader folder on the include path, and makes the plugin depend on them.
function(ch_add_plugin_shaders PLUGIN_TARGET)
  set(SHADER_TARGET ${PLUGIN_TARGET}Shaders)
  ch_add_shaders(${SHADER_TARGET} "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
    OUTPUT_DIR "${CH_PLUGIN_OUTPUT_DIR}/Shaders/${PLUGIN_TARGET}"
    INCLUDE_DIRS "${CH_CORE_SHADER_SOURCE_DIR}")
  get_target_property(PLUGIN_FOLDER ${PLUGIN_TARGET} FOLDER)
  if(PLUGIN_FOLDER)
    set_target_properties(${SHADER_TARGET} PROPERTIES FOLDER "${PLUGIN_FOLDER}")
  endif()
  add_dependencies(${PLUGIN_TARGET} ${SHADER_TARGET})
endfunction()
