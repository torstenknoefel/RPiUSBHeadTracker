# CMakeList.txt : CMake project for RPiUSBHeadTracker, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.24 FATAL_ERROR)

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

target_include_directories(RPiUSBHeadTracker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} /usr/include/opencv4 /usr/local/include /usr/include/libcamera)

# Libraries from GitHub, downloaded by CMake at configure time into <build dir>/_deps.
# Each one is pinned to a fixed commit and checked against its SHA256.
# To update a library, change its commit and hash here.
include(FetchContent)

# taocpp PEGTL, JSON and config. config looks for JSON and JSON looks for PEGTL with
# find_package(); OVERRIDE_FIND_PACKAGE makes them use the copies fetched here.
FetchContent_Declare(pegtl
  URL      https://github.com/taocpp/PEGTL/archive/54a2e32bf4593ed86782b4882702286cc8d621f9.tar.gz
  URL_HASH SHA256=9b710157b83ecde4d72382e3c04ddf42dbcfa1fe40b2fff28efd63443b113531
  OVERRIDE_FIND_PACKAGE
)
FetchContent_Declare(taocpp-json
  URL      https://github.com/taocpp/json/archive/11a31e12eda35c1322f9cf6ebb4cca0653d579dc.tar.gz
  URL_HASH SHA256=487c93b11424e5bc4b4a53320b15fa4da16040425af802373e5351bfabb02500
  OVERRIDE_FIND_PACKAGE
)
FetchContent_Declare(taocpp-config
  URL      https://github.com/taocpp/config/archive/93a455e103cdd8030e770dfff4e2d64d9057ed46.tar.gz
  URL_HASH SHA256=cc7311ee33bdaba9db478b76f3bf2e06098af046db3bc2464ef54f070254906f
)

# Crow web server. Needs the standalone asio headers (Debian: libasio-dev).
FetchContent_Declare(crow
  URL      https://github.com/CrowCpp/Crow/archive/6ba7d7f167223a0ec3b8aa3aeef252ffc8ccd02d.tar.gz
  URL_HASH SHA256=6f044129b86884a102f66a87def4978b7da8a080330163ef965e6ae87b4dcb83
)
set(CROW_INSTALL OFF CACHE BOOL "" FORCE)

# LCCV camera library, built as a static library and linked into the program.
FetchContent_Declare(lccv
  URL      https://github.com/kbarni/LCCV/archive/bca9b3d30cf7e9ff9958c6fbb427b0a8c11a38f2.tar.gz
  URL_HASH SHA256=d36c5344679ec8b549c510b37ce684669d7683abedef066d91d72b1f8d6e2d53
)
# LCCV declares option(BUILD_SHARED_LIBS ON); with CMP0077 NEW it keeps this value.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(pegtl taocpp-json taocpp-config crow lccv)

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

#find_package(Drogon CONFIG REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE "libdrogon.a" "libtrantor.a"  "libz.a" "libssl.a" "libcrypto.a" "libyaml-cpp.a" "jsoncpp" "cares" "brotlicommon" "brotlidec" "brotlienc" "uuid")
target_link_libraries(${PROJECT_NAME} PRIVATE libssl.a ${OpenCV_LIBS} liblccv Crow::Crow taocpp::config)




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
