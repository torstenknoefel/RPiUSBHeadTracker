# Run by FetchContent (PATCH_COMMAND) in LCCV's source folder after the download.
# Makes the same LCCV build against libcamera 0.5 (bookworm) and 0.8 (trixie):
#
# - libcamera 0.8 renamed some control values: MeteringCentreWeighted ->
#   AeMeteringModeCentreWeighted, ExposureNormal -> AeExposureModeNormal,
#   AwbAuto -> AwbModeAuto and so on. LCCV uses the old names in
#   include/lccv.hpp; for libcamera 0.8 and newer the old names are added back.
# - libcamera 0.8 takes std::span for array controls, libcamera 0.5 only its own
#   libcamera::Span. LCCV passes std::span (src/libcamera_app.cpp); that now
#   goes through lccv_compat::ControlSpan, which is whichever one fits.
#
# Safe to run more than once. Stops with an error if LCCV changed so much
# that a piece of text to replace is not there any more.

# Replaces 'from' with 'to' in 'file'; 'from' must be there.
function(replace_checked file from to)
  file(READ "${file}" _text)
  string(FIND "${_text}" "${from}" _pos)
  if (_pos EQUAL -1)
    message(FATAL_ERROR "patch_lccv.cmake: text not found in ${file}, LCCV changed, check the patch:\n${from}")
  endif()
  string(REPLACE "${from}" "${to}" _text "${_text}")
  file(WRITE "${file}" "${_text}")
endfunction()

file(READ "include/lccv.hpp" _header)
if (_header MATCHES "RPIHT_LIBCAMERA_COMPAT")
  return()
endif()

replace_checked("include/lccv.hpp" "#include <libcamera/control_ids.h>\n" [=[#include <libcamera/control_ids.h>
#include <libcamera/version.h>
#include <span>

// RPIHT_LIBCAMERA_COMPAT: added by RPiUSBHeadTracker (cmake/patch_lccv.cmake)
// so this LCCV builds against libcamera 0.5 and 0.8.
#if LIBCAMERA_VERSION_MAJOR > 0 || LIBCAMERA_VERSION_MINOR >= 8
// libcamera 0.8 renamed these control values; give LCCV the old names back.
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
namespace lccv_compat {
template <typename T, std::size_t N> using ControlSpan = std::span<T, N>;
}
#else
// Before 0.8, array controls take libcamera's own Span, not std::span.
namespace lccv_compat {
template <typename T, std::size_t N> using ControlSpan = libcamera::Span<T, N>;
}
#endif
]=])

replace_checked("src/libcamera_app.cpp"
  "controls_.set(controls::FrameDurationLimits, std::span<const int64_t, 2>(limits));"
  "controls_.set(controls::FrameDurationLimits, lccv_compat::ControlSpan<const int64_t, 2>(limits));")
replace_checked("src/libcamera_app.cpp"
  "controls_.set(controls::ColourGains, std::span<const float, 2>(gains));"
  "controls_.set(controls::ColourGains, lccv_compat::ControlSpan<const float, 2>(gains));")

message(STATUS "LCCV patched for libcamera 0.5 and 0.8")
