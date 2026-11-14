#pragma once
#include <cstdint>

// Turns a head angle (degrees, relative to the centre position) into a
// joystick axis value -127 ... 127.
struct AxisMapping
{
  double range = 20.0;    // degrees of head movement for full joystick deflection
  double deadzone = 1.5;  // degrees around the centre that give 0
  double curve = 1.0;     // 1 = linear; > 1 = finer near the centre, faster at the edge
  bool invert = false;
};

int8_t map_axis(double angle, const AxisMapping& m);

// While the face is lost: 1.0 (hold the last value) for 'hold' seconds, then
// falling linearly to 0.0 (stick back in the centre) over 'fade' seconds.
double lost_face_factor(double seconds_lost, double hold = 0.5, double fade = 1.0);
