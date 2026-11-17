ch_add_test(chUtilitiesTest SOURCE chUtilitiesMain.cpp)

target_link_libraries(chUtilitiesTest PRIVATE chUtilities)

# SYSTEM keeps our warning level out of catch.hpp.
target_include_directories(chUtilitiesTest SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/Externals/catch)
