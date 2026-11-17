ch_add_plugin(chParticles FOLDER Plugins ENGINE_PLUGIN)
ch_add_plugin_shaders(chParticles)

target_link_libraries(chParticles PRIVATE chCore)

if(CH_BUILD_TESTS)
  # The plugin exports only loadPlugin, so its test builds the sources in.
  ch_collect_sources(PARTICLE_SOURCES)
  ch_add_test(chParticlesTest SOURCE Test/chParticlesTestMain.cpp)
  target_sources(chParticlesTest PRIVATE ${PARTICLE_SOURCES})
  ch_include_subfolders(chParticlesTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
  target_link_libraries(chParticlesTest PRIVATE chCore)
  target_include_directories(chParticlesTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/Externals/catch)
  target_compile_definitions(chParticlesTest PRIVATE
    CH_TEST_FONT_FILE="${CMAKE_SOURCE_DIR}/chCore/Content/Fonts/Inter-Regular.ttf")
endif()
