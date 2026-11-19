ch_add_test(chUtilitiesTest DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(chUtilitiesTest PRIVATE chUtilities)
