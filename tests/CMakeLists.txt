# Unit tests for the parts that need no camera, web server or USB gadget.
# The sources under test are compiled directly into the test executable.

add_executable(rpiht_tests
  test_QueryParser.cpp
  test_RPiHTConfig.cpp
  test_USBJoystick.cpp
  test_HeadPose.cpp
  test_OneEuroFilter.cpp
  test_JoystickMapping.cpp
  test_TrackerSettings.cpp
  test_Trial.cpp
  test_NeuralPoseMath.cpp
  test_WifiSetup.cpp
  ${PROJECT_SOURCE_DIR}/Command.cpp
  ${PROJECT_SOURCE_DIR}/WifiSetup.cpp
  ${PROJECT_SOURCE_DIR}/QueryParser.cpp
  ${PROJECT_SOURCE_DIR}/RPiHTConfig.cpp
  ${PROJECT_SOURCE_DIR}/USBJoystick.cpp
  ${PROJECT_SOURCE_DIR}/HeadPose.cpp
  ${PROJECT_SOURCE_DIR}/JoystickMapping.cpp
  ${PROJECT_SOURCE_DIR}/TrackerSettings.cpp
  ${PROJECT_SOURCE_DIR}/Trial.cpp
  ${PROJECT_SOURCE_DIR}/NeuralPoseMath.cpp
)

set_property(TARGET rpiht_tests PROPERTY CXX_STANDARD 20)
target_include_directories(rpiht_tests PRIVATE ${PROJECT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(rpiht_tests PRIVATE GTest::gtest_main taocpp::config ${OpenCV_LIBS})

include(GoogleTest)
gtest_discover_tests(rpiht_tests)
