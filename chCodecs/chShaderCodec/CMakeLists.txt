# The DirectX Shader Compiler comes with the Vulkan SDK (the same one chShaders.cmake runs as
# dxc). The codec opens it at run time, so only its header is needed to build, and the
# library is copied into Codecs/ShaderCompiler: the editor loads every library of Codecs
# itself as a codec, and packages never copy that folder.
find_path(CH_DXC_INCLUDE_DIR dxcapi.h
  HINTS "$ENV{VULKAN_SDK}/Include/dxc" "$ENV{VULKAN_SDK}/include/dxc" /usr/include/dxc
  DOC "Folder of dxcapi.h, shipped with the Vulkan SDK")
if(WIN32)
  find_file(CH_DXCOMPILER_LIBRARY dxcompiler.dll
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    DOC "dxcompiler.dll, shipped with the Vulkan SDK")
else()
  find_file(CH_DXCOMPILER_LIBRARY libdxcompiler.so
    HINTS "$ENV{VULKAN_SDK}/lib" /usr/lib /usr/lib64 /usr/local/lib
    DOC "libdxcompiler.so, shipped with the Vulkan SDK")
endif()

if(NOT CH_DXC_INCLUDE_DIR)
  message(WARNING "dxcapi.h not found: chShaderCodec is not built, so the editor cannot "
                  "import shaders. Install the Vulkan SDK and set VULKAN_SDK.")
  return()
endif()

ch_add_plugin(chShaderCodec FOLDER Codecs CODEC)
target_link_libraries(chShaderCodec PRIVATE chCore)
target_include_directories(chShaderCodec SYSTEM PRIVATE "${CH_DXC_INCLUDE_DIR}")

if(CH_DXCOMPILER_LIBRARY)
  add_custom_command(TARGET chShaderCodec POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:chShaderCodec>/ShaderCompiler"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CH_DXCOMPILER_LIBRARY}"
            "$<TARGET_FILE_DIR:chShaderCodec>/ShaderCompiler"
    COMMENT "Copying the DirectX Shader Compiler next to chShaderCodec"
    VERBATIM)
else()
  message(WARNING "The dxcompiler library was not found: chShaderCodec will look for it on "
                  "the system library path at run time.")
endif()
