# Run by FetchContent (PATCH_COMMAND) in LCCV's source folder after the download.
#
# libcamera 0.8 (Raspberry Pi OS trixie) renamed some control values:
# MeteringCentreWeighted -> AeMeteringModeCentreWeighted, ExposureNormal ->
# AeExposureModeNormal, AwbAuto -> AwbModeAuto and so on. LCCV still uses the
# old names in include/lccv.hpp. This adds the old names back for libcamera
# 0.8 and newer, so the same LCCV builds against libcamera 0.5 (bookworm) and
# 0.8 (trixie). Safe to run more than once.
set(_file "include/lccv.hpp")
file(READ "${_file}" _text)
if (_text MATCHES "RPIHT_LIBCAMERA_OLD_NAMES")
  return()
endif()

set(_anchor "#include <libcamera/control_ids.h>\n")
string(FIND "${_text}" "${_anchor}" _pos)
if (_pos EQUAL -1)
  message(FATAL_ERROR "patch_lccv.cmake: '${_anchor}' not found in ${_file}; LCCV changed, check the patch")
endif()

set(_compat [=[#include <libcamera/control_ids.h>
#include <libcamera/version.h>

// RPIHT_LIBCAMERA_OLD_NAMES: added by RPiUSBHeadTracker (cmake/patch_lccv.cmake).
// libcamera 0.8 renamed these control values; give LCCV the old names back.
#if LIBCAMERA_VERSION_MAJOR > 0 || LIBCAMERA_VERSION_MINOR >= 8
namespace libcamera::controls {
inline constexpr auto MeteringCentreWeighted = AeMeteringModeCentreWeighted;
inline constexpr auto MeteringSpot           = AeMeteringModeSpot;
inline constexpr auto MeteringMatrix         = AeMeteringModeMatrix;
inline constexpr auto MeteringCustom         = AeMeteringModeCustom;
inline constexpr auto ExposureNormal         = AeExposureModeNormal;
inline constexpr auto ExposureShort          = AeExposureModeShort;
inline constexpr auto ExposureCustom         = AeExposureModeCustom;
inline constexpr auto AwbAuto                = AwbModeAuto;
inline constexpr auto AwbIncandescent        = AwbModeIncandescent;
inline constexpr auto AwbTungsten            = AwbModeTungsten;
inline constexpr auto AwbFluorescent         = AwbModeFluorescent;
inline constexpr auto AwbIndoor              = AwbModeIndoor;
inline constexpr auto AwbDaylight            = AwbModeDaylight;
inline constexpr auto AwbCloudy              = AwbModeCloudy;
inline constexpr auto AwbCustom              = AwbModeCustom;
}
#endif
]=])

string(REPLACE "${_anchor}" "${_compat}" _text "${_text}")
file(WRITE "${_file}" "${_text}")
message(STATUS "LCCV patched for libcamera 0.8 control names")
