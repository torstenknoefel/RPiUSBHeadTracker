#include "TrackerSettings.h"

#include <algorithm>

#include "HeadPose.h"

TrackerSettings tracker_settings(const RPiHTConfigData& d)
{
  TrackerSettings s;
  s.x.range = std::max(d.track_range_x, 1.0);
  s.y.range = std::max(d.track_range_y, 1.0);
  s.x.deadzone = s.y.deadzone = std::max(d.track_deadzone, 0.0);
  s.x.curve = s.y.curve = std::max(d.track_curve, 0.1);
  s.x.invert = d.track_invert_x;
  s.y.invert = d.track_invert_y;
  s.filter_min_cutoff = std::max(d.track_smoothing, 0.05);
  s.filter_beta = std::max(d.track_speed, 0.0);
  s.detect_size = detect_size_for(d.track_detect_width, cv::Size(int(d.cam_width), int(d.cam_height)));
  return s;
}
