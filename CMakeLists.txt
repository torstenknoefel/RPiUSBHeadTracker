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
add_executable (RPiUSBHeadTracker "RPiUSBHeadTracker.cpp" "RPiUSBHeadTracker.h" "USBJoystick.h" "USBJoystick.cpp" "RPiHTWebServer.cpp" "RPiHTConfig.h" "RPiHTConfig.cpp" "RPiHTWebApp.h" "RPiHTWebApp.cpp" "QueryParser.h" "QueryParser.cpp"  "RPiHTCam.cpp" "RPiHTCam.h" "CmdParser.h" "HeadTracker.h" "HeadTracker.cpp" "TrackerSettings.h" "TrackerSettings.cpp" "Trial.h" "Trial.cpp" "HeadPose.h" "HeadPose.cpp" "NeuralPose.h" "NeuralPose.cpp" "NeuralPoseMath.h" "NeuralPoseMath.cpp" "JoystickMapping.h" "JoystickMapping.cpp" "OneEuroFilter.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RPiUSBHeadTracker PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(RPiUSBHeadTracker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} /usr/include/opencv4 /usr/local/include /usr/include/libcamera)

# Libraries from GitHub, downloaded by CMake at configure time into <build dir>/_deps.
# Each one is pinned to a fixed commit and checked against its SHA256.
# Each library has two addresses, tried in this order:
#   1. our fork under github.com/torstenknoefel (stays available if upstream is deleted)
#   2. the upstream repository
# To update a library: "Sync fork" on GitHub, then change its commit and hash here.
include(FetchContent)

# taocpp PEGTL, JSON and config. config looks for JSON and JSON looks for PEGTL with
# find_package(); OVERRIDE_FIND_PACKAGE makes them use the copies fetched here.
FetchContent_Declare(pegtl
  URL      https://github.com/torstenknoefel/PEGTL/archive/54a2e32bf4593ed86782b4882702286cc8d621f9.tar.gz
           https://github.com/taocpp/PEGTL/archive/54a2e32bf4593ed86782b4882702286cc8d621f9.tar.gz
  URL_HASH SHA256=9b710157b83ecde4d72382e3c04ddf42dbcfa1fe40b2fff28efd63443b113531
  OVERRIDE_FIND_PACKAGE
)
FetchContent_Declare(taocpp-json
  URL      https://github.com/torstenknoefel/json/archive/11a31e12eda35c1322f9cf6ebb4cca0653d579dc.tar.gz
           https://github.com/taocpp/json/archive/11a31e12eda35c1322f9cf6ebb4cca0653d579dc.tar.gz
  URL_HASH SHA256=487c93b11424e5bc4b4a53320b15fa4da16040425af802373e5351bfabb02500
  OVERRIDE_FIND_PACKAGE
)
FetchContent_Declare(taocpp-config
  URL      https://github.com/torstenknoefel/config/archive/93a455e103cdd8030e770dfff4e2d64d9057ed46.tar.gz
           https://github.com/taocpp/config/archive/93a455e103cdd8030e770dfff4e2d64d9057ed46.tar.gz
  URL_HASH SHA256=cc7311ee33bdaba9db478b76f3bf2e06098af046db3bc2464ef54f070254906f
)

# Crow web server. Needs the standalone asio headers (Debian: libasio-dev).
FetchContent_Declare(crow
  URL      https://github.com/torstenknoefel/Crow/archive/6ba7d7f167223a0ec3b8aa3aeef252ffc8ccd02d.tar.gz
           https://github.com/CrowCpp/Crow/archive/6ba7d7f167223a0ec3b8aa3aeef252ffc8ccd02d.tar.gz
  URL_HASH SHA256=6f044129b86884a102f66a87def4978b7da8a080330163ef965e6ae87b4dcb83
)
set(CROW_INSTALL OFF CACHE BOOL "" FORCE)

# LCCV camera library, built as a static library and linked into the program.
FetchContent_Declare(lccv
  URL      https://github.com/torstenknoefel/LCCV/archive/bca9b3d30cf7e9ff9958c6fbb427b0a8c11a38f2.tar.gz
           https://github.com/kbarni/LCCV/archive/bca9b3d30cf7e9ff9958c6fbb427b0a8c11a38f2.tar.gz
  URL_HASH SHA256=d36c5344679ec8b549c510b37ce684669d7683abedef066d91d72b1f8d6e2d53
)
# LCCV declares option(BUILD_SHARED_LIBS ON); with CMP0077 NEW it keeps this value.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(pegtl taocpp-json taocpp-config crow lccv)

# Downloads a model file into <build dir>/models (where the program loads it
# from), unless it is already there with the right SHA256. The URLs are tried
# in order.
function(download_model name sha256 problem)
  set(_file "${CMAKE_CURRENT_BINARY_DIR}/models/${name}")
  if (EXISTS "${_file}")
    file(SHA256 "${_file}" _hash)
    if (_hash STREQUAL sha256)
      return()
    endif()
  endif()
  foreach(_url IN LISTS ARGN)
    message(STATUS "Downloading ${name} from ${_url}")
    file(DOWNLOAD "${_url}" "${_file}" EXPECTED_HASH SHA256=${sha256} STATUS _status)
    list(GET _status 0 _code)
    if (_code EQUAL 0)
      return()
    endif()
    message(WARNING "Download of ${name} failed: ${_status}")
  endforeach()
  message(WARNING "${name} missing - ${problem}")
endfunction()

# Face detection model for the head tracker: YuNet, 2022 version (the 2023
# version needs OpenCV 4.8; Debian bookworm has 4.6). From the OpenCV model zoo
# (MIT licence), stored with Git LFS, pinned to a commit and checked by SHA256.
download_model(face_detection_yunet_2022mar.onnx
  50ef07f702a31741ca46a4c0d947773b64143b9362780237bf0d427d6c79bab7
  "the head tracker will not work"
  https://media.githubusercontent.com/media/opencv/opencv_zoo/7e062e54cf5410c09b795ff71b4a255e58498c79/models/face_detection_yunet/face_detection_yunet_2022mar.onnx
)

# Second face model, "NeuralNet": the head pose models of the opentrack NeuralNet
# tracker (by Michael Welter, https://github.com/opentrack/opentrack, ISC licence),
# run with ONNX Runtime (MIT licence). Measures nodding much better than YuNet's
# five points, but needs more CPU. ONNX Runtime is downloaded as the ready-built
# package from its GitHub release (64-bit ARM Linux only), pinned and checked by
# SHA256; there is no fork for release packages.
option(RPIHT_NEURALNET "Build the NeuralNet face model (downloads ONNX Runtime)" ON)
set(RPIHT_HAVE_NEURALNET OFF)
if (RPIHT_NEURALNET)
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    FetchContent_Declare(onnxruntime
      URL      https://github.com/microsoft/onnxruntime/releases/download/v1.32.0/onnxruntime-linux-aarch64-1.32.0.tgz
      URL_HASH SHA256=81a46251dd90da4122f4b9ceb4433f836fbc79ee1eddffca7c09033af81a1939
    )
    FetchContent_MakeAvailable(onnxruntime)  # just unpacks: the package has no CMakeLists.txt
    add_library(onnxruntime_prebuilt SHARED IMPORTED)
    set_target_properties(onnxruntime_prebuilt PROPERTIES
      IMPORTED_LOCATION "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.1.32.0"
      INTERFACE_INCLUDE_DIRECTORIES "${onnxruntime_SOURCE_DIR}/include"
    )
    set(_opentrack https://raw.githubusercontent.com/opentrack/opentrack/03a0e69b02a11c425e2f07c728686ff7f2d6517a/tracker-neuralnet/models)
    download_model(head-localizer.onnx
      f26679fe5e01a08dab0b3b9b586b613c68622775e0b8e14ddae53f30391f7402
      "the NeuralNet face model will not work" ${_opentrack}/head-localizer.onnx)
    download_model(head-pose-0.5-small.onnx
      7c14f84114fb9eca89759d8a36350c6faae2b4187258cae07afb77a93c2d7eec
      "the NeuralNet face model will not work" ${_opentrack}/head-pose-0.5-small.onnx)
    set(RPIHT_HAVE_NEURALNET ON)
  else()
    message(STATUS "NeuralNet face model: no ONNX Runtime package for ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} - only YuNet is built")
  endif()
endif()

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
if (RPIHT_HAVE_NEURALNET)
  target_compile_definitions(${PROJECT_NAME} PRIVATE RPIHT_HAVE_NEURALNET=1)
  target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime_prebuilt)
endif()




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
