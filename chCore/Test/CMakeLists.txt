# Tests internal classes too (ShadowRenderer, ModelSerializer).
ch_add_test(chCoreTest SOURCE chCoreTestMain.cpp PRIVATE_ACCESS chCore)

# chCore brings the CH_*_ENABLED defines its headers depend on.
target_link_libraries(chCoreTest PRIVATE chCore)

# SYSTEM keeps our warning level out of catch.hpp.
target_include_directories(chCoreTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/Externals/catch)

# The font tests read the engine's default font straight from the source tree.
target_compile_definitions(chCoreTest PRIVATE
  CH_TEST_FONT_FILE="${CMAKE_SOURCE_DIR}/chCore/Content/Fonts/Inter-Regular.ttf")

# Every test is in one file (until they are split by topic), which has more sections than an
# MSVC object file holds by default.
if(MSVC)
  target_compile_options(chCoreTest PRIVATE /bigobj)
endif()
