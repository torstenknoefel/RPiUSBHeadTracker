# CMakeList.txt : CMake project for RPiUSBHeadTracker, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8 FATAL_ERROR)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (RPiUSBHeadTracker LANGUAGES CXX)
add_compile_definitions(ASIO_STANDALONE)
# Add source to this project's executable.
add_executable (RPiUSBHeadTracker "RPiUSBHeadTracker.cpp" "RPiUSBHeadTracker.h" "USBJoystick.h" "USBJoystick.cpp" "RPiHTWebServer.cpp" "RPiHTConfig.h" "RPiHTConfig.cpp" "RPiHTWebApp.h" "RPiHTWebApp.cpp" "QueryParser.h" "QueryParser.cpp"  "RPiHTCam.cpp" "RPiHTCam.h" "CmdParser.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RPiUSBHeadTracker PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(RPiUSBHeadTracker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include /usr/include/opencv4 /usr/local/include /usr/include/libcamera)

find_package( OpenCV REQUIRED )

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/templates")
add_custom_target(templates
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${CMAKE_CURRENT_SOURCE_DIR}/templates/*"
	#"$<TARGET_FILE_DIR:RPiUSBHeadTracker>/templates"
	"${CMAKE_CURRENT_BINARY_DIR}/templates"
	COMMENT "Copying templates"
)
add_dependencies( RPiUSBHeadTracker templates)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/static")
add_custom_target(staticfiles
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${CMAKE_CURRENT_SOURCE_DIR}/static/*"
	#"$<TARGET_FILE_DIR:RPiUSBHeadTracker>/templates"
	"${CMAKE_CURRENT_BINARY_DIR}/static"
	COMMENT "Copying static files"
)
add_dependencies( RPiUSBHeadTracker staticfiles)

add_subdirectory(./LCCV)

#find_package(Drogon CONFIG REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE "libdrogon.a" "libtrantor.a"  "libz.a" "libssl.a" "libcrypto.a" "libyaml-cpp.a" "jsoncpp" "cares" "brotlicommon" "brotlidec" "brotlienc" "uuid")
target_link_libraries(${PROJECT_NAME} PRIVATE libssl.a ${OpenCV_LIBS} liblccv.so)




# Unit tests (GoogleTest; on the Pi: sudo apt install libgtest-dev). Run with: ctest --test-dir <build dir>
option(RPIHT_BUILD_TESTS "Build the unit tests" ON)
if (RPIHT_BUILD_TESTS)
  find_package(GTest QUIET)
  if (GTest_FOUND)
    enable_testing()
    add_subdirectory(tests)
  else()
    message(STATUS "GoogleTest not found - unit tests are not built")
  endif()
endif()

# TODO: Add install targets if needed.
