# bc7enc_rdo's encoders (BC7, and BC1, BC4, BC5 through rgbcx) plus the BC7 decoder the tests
# read back with. Built apart and without our warning flags: it is third-party code.
set(CH_BC7ENC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Externals/bc7enc_rdo")
add_library(chBc7enc STATIC
  "${CH_BC7ENC_DIR}/bc7enc.cpp"
  "${CH_BC7ENC_DIR}/rgbcx.cpp"
  "${CH_BC7ENC_DIR}/bc7decomp.cpp")
target_include_directories(chBc7enc SYSTEM PUBLIC "${CH_BC7ENC_DIR}")
set_target_properties(chBc7enc PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  FOLDER "Externals/bc7enc_rdo")

ch_add_plugin(chTextureCompressor FOLDER Codecs CODEC)
target_link_libraries(chTextureCompressor PRIVATE chCore chBc7enc)

if(CH_BUILD_TESTS)
  # The plugin exports only loadPlugin, so its test builds the sources in.
  ch_collect_sources(TEXTURE_COMPRESSOR_SOURCES)
  ch_add_test(chTextureCompressorTest DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Test")
  target_sources(chTextureCompressorTest PRIVATE ${TEXTURE_COMPRESSOR_SOURCES})
  ch_include_subfolders(chTextureCompressorTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
  target_link_libraries(chTextureCompressorTest PRIVATE chCore chBc7enc)
endif()
