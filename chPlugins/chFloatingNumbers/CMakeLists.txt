ch_add_plugin(chFloatingNumbers FOLDER Plugins ENGINE_PLUGIN)
ch_add_plugin_shaders(chFloatingNumbers)

target_link_libraries(chFloatingNumbers PRIVATE chCore)

if(CH_BUILD_TESTS)
  # The plugin exports only loadPlugin, so its test builds the sources in.
  ch_collect_sources(FLOATING_NUMBER_SOURCES)
  ch_add_test(chFloatingNumbersTest SOURCE Test/chFloatingNumbersTestMain.cpp)
  target_sources(chFloatingNumbersTest PRIVATE ${FLOATING_NUMBER_SOURCES})
  ch_include_subfolders(chFloatingNumbersTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
  target_link_libraries(chFloatingNumbersTest PRIVATE chCore)
  target_include_directories(chFloatingNumbersTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/Externals/catch)
  target_compile_definitions(chFloatingNumbersTest PRIVATE
    CH_TEST_FONT_FILE="${CMAKE_SOURCE_DIR}/chCore/Content/Fonts/Inter-Regular.ttf")
endif()
