# Unit tests for the parts that need no camera, web server or USB gadget.
# The sources under test are compiled directly into the test executable.

add_executable(rpiht_tests
  test_QueryParser.cpp
  test_RPiHTConfig.cpp
  test_USBJoystick.cpp
  ${PROJECT_SOURCE_DIR}/QueryParser.cpp
  ${PROJECT_SOURCE_DIR}/RPiHTConfig.cpp
  ${PROJECT_SOURCE_DIR}/USBJoystick.cpp
)

set_property(TARGET rpiht_tests PROPERTY CXX_STANDARD 20)
target_include_directories(rpiht_tests PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(rpiht_tests PRIVATE GTest::gtest_main taocpp::config)

include(GoogleTest)
gtest_discover_tests(rpiht_tests)
