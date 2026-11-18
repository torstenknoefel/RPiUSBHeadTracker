# CMakeList.txt : CMake project for RPiUSBHeadTracker, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.24 FATAL_ERROR)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# The version shown on the Help page; release tags must match it (vX.Y.Z)
project (RPiUSBHeadTracker VERSION 0.1.0 LANGUAGES CXX)
add_compile_definitions(ASIO_STANDALONE)
# Add source to this project's executable.
add_executable (RPiUSBHeadTracker "RPiUSBHeadTracker.cpp" "RPiUSBHeadTracker.h" "USBJoystick.h" "USBJoystick.cpp" "RPiHTWebServer.cpp" "RPiHTConfig.h" "RPiHTConfig.cpp" "RPiHTWebApp.h" "RPiHTWebApp.cpp" "QueryParser.h" "QueryParser.cpp"  "RPiHTCam.cpp" "RPiHTCam.h" "CmdParser.h" "HeadTracker.h" "HeadTracker.cpp" "TrackerSettings.h" "TrackerSettings.cpp" "Trial.h" "Trial.cpp" "HeadPose.h" "HeadPose.cpp" "NeuralPose.h" "NeuralPose.cpp" "NeuralPoseMath.h" "NeuralPoseMath.cpp" "JoystickMapping.h" "JoystickMapping.cpp" "OneEuroFilter.h" "Command.h" "Command.cpp" "WifiSetup.h" "WifiSetup.cpp")

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
# cmake/patch_lccv.cmake makes it build against libcamera 0.5 (bookworm) and 0.8 (trixie).
FetchContent_Declare(lccv
  URL      https://github.com/torstenknoefel/LCCV/archive/d246763350f6e9eff2fca38135f68d58e0eb0445.tar.gz
           https://github.com/kbarni/LCCV/archive/d246763350f6e9eff2fca38135f68d58e0eb0445.tar.gz
  URL_HASH SHA256=6d5c72fa8e4185aedc914a9479e7021e23b58c1cae963f18e60541b49df8d7ab
  PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_lccv.cmake
)
# LCCV declares option(BUILD_SHARED_LIBS ON); with CMP0077 NEW it keeps this value.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_EXAMPLES OFF)  # LCCV's example programs

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

# Face detection model for the head tracker: YuNet from the OpenCV model zoo
# (MIT licence), stored with Git LFS, pinned to a commit and checked by SHA256.
# Which version depends on OpenCV: up to 4.7 (bookworm: 4.6) only reads the
# 2022 model, 4.8 and newer (trixie: 4.10) only the 2023 one. HeadTracker.cpp
# picks the same file by OpenCV version.
find_package(OpenCV REQUIRED)
if (OpenCV_VERSION VERSION_LESS 4.8)
  download_model(face_detection_yunet_2022mar.onnx
    50ef07f702a31741ca46a4c0d947773b64143b9362780237bf0d427d6c79bab7
    "the head tracker will not work"
    https://media.githubusercontent.com/media/opencv/opencv_zoo/7e062e54cf5410c09b795ff71b4a255e58498c79/models/face_detection_yunet/face_detection_yunet_2022mar.onnx
  )
else()
  download_model(face_detection_yunet_2023mar.onnx
    8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4
    "the head tracker will not work"
    https://media.githubusercontent.com/media/opencv/opencv_zoo/f12e12798e8314f7c074a6656816c048dcc95b7a/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
  )
endif()

# Second face model, "NeuralNet": the head pose models of the opentrack NeuralNet
# tracker (by Michael Welter, https://github.com/opentrack/opentrack, ISC licence),
# run with ONNX Runtime (MIT licence). Measures nodding much better than YuNet's
# five points, but needs more CPU. ONNX Runtime comes ready-built (64-bit ARM
# Linux only), pinned and checked by SHA256, from Microsoft's NuGet package
# Microsoft.ML.OnnxRuntime: its GitHub repository (and with it the release
# packages used before) disappeared on 2026-11-18 without explanation.
# Version 1.30.0 (published 2026-09-10), not the newest 1.32.0 (2026-11-13, five
# days before): until Microsoft explains, a release from well before is the safer
# choice. Its linux-arm64 library is byte for byte the same in the NuGet and in the
# Maven Central package (SHA256 64e903a4...). The NuGet package holds all
# platforms (157 MB); only the Linux ARM library and the C/C++ headers are used.
option(RPIHT_NEURALNET "Build the NeuralNet face model (downloads ONNX Runtime)" ON)
set(RPIHT_HAVE_NEURALNET OFF)
if (RPIHT_NEURALNET)
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    FetchContent_Declare(onnxruntime
      URL      https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.30.0/microsoft.ml.onnxruntime.1.30.0.nupkg
      URL_HASH SHA256=30df119bd9164eaa187666d93147448fdd4a9a1a3caf83f1505daf7af2d1c5d2
      DOWNLOAD_NAME onnxruntime-1.30.0.zip  # a .nupkg is a zip file; the name tells CMake how to unpack it
    )
    FetchContent_MakeAvailable(onnxruntime)  # just unpacks: the package has no CMakeLists.txt
    set(_ort_lib_dir "${onnxruntime_SOURCE_DIR}/runtimes/linux-arm64/native")
    # The program asks for the library by its soname, libonnxruntime.so.1
    file(CREATE_LINK libonnxruntime.so "${_ort_lib_dir}/libonnxruntime.so.1" SYMBOLIC)
    add_library(onnxruntime_prebuilt SHARED IMPORTED)
    set_target_properties(onnxruntime_prebuilt PROPERTIES
      IMPORTED_LOCATION "${_ort_lib_dir}/libonnxruntime.so"
      IMPORTED_SONAME libonnxruntime.so.1
      INTERFACE_INCLUDE_DIRECTORIES "${onnxruntime_SOURCE_DIR}/build/native/include"
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

# Web page styles and fonts, served by the Pi itself from <build dir>/static/vendor:
# on the tracker's own Wi-Fi (hotspot) the phone has no internet, so nothing may
# come from a CDN. From the npm registry, pinned and checked by SHA256 (no fork
# for release packages, as for ONNX Runtime):
#   BeerCSS (MIT licence) with the Material Symbols icon fonts (Apache 2.0)
#   Barlow Condensed from Fontsource (SIL Open Font License 1.1)
FetchContent_Declare(beercss
  URL      https://registry.npmjs.org/beercss/-/beercss-3.12.13.tgz
  URL_HASH SHA256=5cc9f6086d385a3f150e91b2b3c7ac78e2f1f53bedfa84abe3a699277c1dbcb2
)
FetchContent_Declare(barlow_condensed
  URL      https://registry.npmjs.org/@fontsource/barlow-condensed/-/barlow-condensed-5.2.6.tgz
  URL_HASH SHA256=b5380732d6b60749237a908ac6a8c117676fb82509440768bbc0d83a8835a538
)
FetchContent_MakeAvailable(beercss barlow_condensed)  # just unpacks: no CMakeLists.txt
set(_vendor "${CMAKE_CURRENT_BINARY_DIR}/static/vendor")
file(MAKE_DIRECTORY "${_vendor}")
# The stylesheet looks for its icon fonts next to itself first and on the CDN
# second; the CDN address is taken out, so the pages never reach the internet.
file(READ "${beercss_SOURCE_DIR}/dist/cdn/beer.min.css" _beer_css)
string(REPLACE ",url(https://cdn.jsdelivr.net/npm/beercss@3.12.13/dist/cdn/" ",url(" _beer_css "${_beer_css}")
file(WRITE "${_vendor}/beer.min.css" "${_beer_css}")
file(GLOB _beer_files "${beercss_SOURCE_DIR}/dist/cdn/*.svg" "${beercss_SOURCE_DIR}/dist/cdn/*.woff2")
file(COPY ${_beer_files} "${beercss_SOURCE_DIR}/dist/cdn/beer.min.js" DESTINATION "${_vendor}")
file(COPY_FILE "${beercss_SOURCE_DIR}/LICENSE" "${_vendor}/beercss-LICENSE.txt")
file(COPY "${barlow_condensed_SOURCE_DIR}/files/barlow-condensed-latin-500-normal.woff2"
          "${barlow_condensed_SOURCE_DIR}/files/barlow-condensed-latin-500-italic.woff2"
     DESTINATION "${_vendor}")
file(COPY_FILE "${barlow_condensed_SOURCE_DIR}/LICENSE" "${_vendor}/barlow-condensed-LICENSE.txt")


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

# No OpenSSL: the web server has no HTTPS (Crow without CROW_ENABLE_SSL)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} liblccv Crow::Crow taocpp::config)
target_compile_definitions(${PROJECT_NAME} PRIVATE RPIHT_VERSION="${PROJECT_VERSION}")
if (RPIHT_HAVE_NEURALNET)
  target_compile_definitions(${PROJECT_NAME} PRIVATE RPIHT_HAVE_NEURALNET=1)
  target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime_prebuilt)
endif()

# Installing, for the Debian package:
#   cmake --install <build dir> --component rpi-headtracker --prefix /usr
# Only this component: the fetched libraries (PEGTL, LCCV) install their own
# headers and files, which are not needed at run time.
# The program then runs with -d <prefix>/share/rpi-headtracker:
#   bin/RPiUSBHeadTracker
#   share/rpi-headtracker/   templates, static (with vendor), models, default.json
#   lib/rpi-headtracker/     ONNX Runtime (private: found through the program's RPATH),
#                            headtracker_device.sh, headtracker-wifi.sh, headtracker-portal.sh
# The Debian packages split these files (debian/*.install): the Wi-Fi scripts go
# into rpi-headtracker-wifi, which also takes its config files from wifi/ and debian/.
# Not distribution-wide lib/<arch>: ONNX Runtime is the project's own copy.
include(GNUInstallDirs)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME rpi-headtracker)
set(RPIHT_SHARE_DIR "${CMAKE_INSTALL_DATADIR}/rpi-headtracker")
set(RPIHT_LIB_DIR "lib/rpi-headtracker")
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../${RPIHT_LIB_DIR}")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY templates/ DESTINATION "${RPIHT_SHARE_DIR}/templates"
        FILES_MATCHING PATTERN "*.html" PATTERN "example_chat.html" EXCLUDE)
install(FILES static/colors.css static/fonts.css static/logo.svg static/carbon-tile.png
        DESTINATION "${RPIHT_SHARE_DIR}/static")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/static/vendor" DESTINATION "${RPIHT_SHARE_DIR}/static")
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/models" DESTINATION "${RPIHT_SHARE_DIR}")
install(FILES config/default.json DESTINATION "${RPIHT_SHARE_DIR}")
install(PROGRAMS headtracker_device.sh wifi/headtracker-wifi.sh wifi/headtracker-portal.sh
        DESTINATION "${RPIHT_LIB_DIR}")
if (RPIHT_HAVE_NEURALNET)
  # The library under the name the program asks for (its soname, libonnxruntime.so.1),
  # and its licence texts
  install(FILES "${_ort_lib_dir}/libonnxruntime.so" DESTINATION "${RPIHT_LIB_DIR}" RENAME libonnxruntime.so.1)
  install(FILES "${onnxruntime_SOURCE_DIR}/LICENSE" DESTINATION "${RPIHT_SHARE_DIR}/licenses" RENAME onnxruntime-LICENSE.txt)
  install(FILES "${onnxruntime_SOURCE_DIR}/ThirdPartyNotices.txt" DESTINATION "${RPIHT_SHARE_DIR}/licenses"
          RENAME onnxruntime-ThirdPartyNotices.txt)
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
