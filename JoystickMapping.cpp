#include "JoystickMapping.h"

#include <algorithm>
#include <cmath>

int8_t map_axis(double angle, const AxisMapping& m)
{
  if (!std::isfinite(angle))
    return 0;
  if (m.invert)
    angle = -angle;

  const double magnitude = std::fabs(angle) - m.deadzone;
  if (magnitude <= 0)
    return 0;

  const double span = std::max(m.range - m.deadzone, 1e-6);
  double n = std::min(magnitude / span, 1.0);       // 0 ... 1
  n = std::pow(n, std::max(m.curve, 0.1));
  const double value = std::round(n * 127.0);
  return static_cast<int8_t>(angle < 0 ? -value : value);
}

double lost_face_factor(double seconds_lost, double hold, double fade)
{
  if (seconds_lost <= hold)
    return 1.0;
  if (fade <= 0)
    return 0.0;
  return std::clamp(1.0 - (seconds_lost - hold) / fade, 0.0, 1.0);
}
