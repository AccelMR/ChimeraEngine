ch_add_plugin(chProjectiles FOLDER Plugins ENGINE_PLUGIN)

target_link_libraries(chProjectiles PRIVATE chCore)

if(CH_BUILD_TESTS)
  # The plugin exports only loadPlugin, so its test builds the sources in.
  ch_collect_sources(PROJECTILE_SOURCES)
  ch_add_test(chProjectilesTest DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Test")
  target_sources(chProjectilesTest PRIVATE ${PROJECTILE_SOURCES})
  ch_include_subfolders(chProjectilesTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
  target_link_libraries(chProjectilesTest PRIVATE chCore)
endif()
