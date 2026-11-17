# Tests internal classes too (ShadowRenderer, ModelSerializer).
ch_add_test(chCoreTest DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE_ACCESS chCore)

# chCore brings the CH_*_ENABLED defines its headers depend on.
target_link_libraries(chCoreTest PRIVATE chCore)

# The font tests read the engine's default font straight from the source tree.
target_compile_definitions(chCoreTest PRIVATE
  CH_TEST_FONT_FILE="${CMAKE_SOURCE_DIR}/chCore/Content/Fonts/Inter-Regular.ttf")

