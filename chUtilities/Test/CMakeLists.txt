ch_add_test(chUtilitiesTest DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(chUtilitiesTest PRIVATE chUtilities)

# Crashes on purpose; the crash handler tests run it and read the report it leaves.
ch_add_executable(chCrashTest FOLDER Tests SOURCES
  "${CMAKE_SOURCE_DIR}/TestSupport/CrashTest/chCrashTestMain.cpp")
target_link_libraries(chCrashTest PRIVATE chUtilities)
add_dependencies(chUtilitiesTest chCrashTest)
