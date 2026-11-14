#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "JoystickMapping.h"

TEST(JoystickMapping, CentreAndDeadzoneGiveZero)
{
  AxisMapping m;  // range 20, deadzone 1.5
  EXPECT_EQ(map_axis(0.0, m), 0);
  EXPECT_EQ(map_axis(1.4, m), 0);
  EXPECT_EQ(map_axis(-1.4, m), 0);
  EXPECT_GT(map_axis(1.6, m), 0);
}

TEST(JoystickMapping, FullRangeGivesFullDeflection)
{
  AxisMapping m;
  EXPECT_EQ(map_axis(20.0, m), 127);
  EXPECT_EQ(map_axis(-20.0, m), -127);
}

TEST(JoystickMapping, BeyondTheRangeIsClamped)
{
  AxisMapping m;
  EXPECT_EQ(map_axis(45.0, m), 127);
  EXPECT_EQ(map_axis(-90.0, m), -127);
}

TEST(JoystickMapping, LinearBetweenDeadzoneAndRange)
{
  AxisMapping m;
  m.deadzone = 0;
  m.range = 20;
  EXPECT_EQ(map_axis(10.0, m), 64);  // half way: round(63.5)
  EXPECT_EQ(map_axis(5.0, m), 32);   // round(31.75)
}

TEST(JoystickMapping, NoJumpAtTheDeadzoneEdge)
{
  AxisMapping m;
  EXPECT_LE(map_axis(m.deadzone + 0.1, m), 2);
}

TEST(JoystickMapping, CurveMakesTheCentreFiner)
{
  AxisMapping linear, curved;
  curved.curve = 2.0;
  EXPECT_LT(map_axis(8.0, curved), map_axis(8.0, linear));
  EXPECT_EQ(map_axis(20.0, curved), 127);
}

TEST(JoystickMapping, InvertFlipsTheSign)
{
  AxisMapping m;
  m.invert = true;
  EXPECT_EQ(map_axis(20.0, m), -127);
  EXPECT_EQ(map_axis(-20.0, m), 127);
}

TEST(JoystickMapping, InvalidAngleGivesZero)
{
  AxisMapping m;
  EXPECT_EQ(map_axis(std::numeric_limits<double>::quiet_NaN(), m), 0);
  EXPECT_EQ(map_axis(std::numeric_limits<double>::infinity(), m), 0);
}

TEST(LostFaceFactor, HoldsThenFadesToZero)
{
  EXPECT_DOUBLE_EQ(lost_face_factor(0.0), 1.0);
  EXPECT_DOUBLE_EQ(lost_face_factor(0.5), 1.0);
  EXPECT_NEAR(lost_face_factor(1.0), 0.5, 1e-9);
  EXPECT_DOUBLE_EQ(lost_face_factor(1.5), 0.0);
  EXPECT_DOUBLE_EQ(lost_face_factor(10.0), 0.0);
}

TEST(LostFaceFactor, ZeroFadeDropsAtOnce)
{
  EXPECT_DOUBLE_EQ(lost_face_factor(0.6, 0.5, 0.0), 0.0);
}
