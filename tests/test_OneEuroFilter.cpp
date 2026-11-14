#include <gtest/gtest.h>

#include <cmath>
#include <random>

#include "OneEuroFilter.h"

TEST(OneEuroFilter, FirstValuePassesUnchanged)
{
  OneEuroFilter f;
  EXPECT_DOUBLE_EQ(f.filter(7.5, 0.0), 7.5);
}

TEST(OneEuroFilter, ConstantInputStaysConstant)
{
  OneEuroFilter f;
  for (int i = 0; i < 100; ++i)
    EXPECT_NEAR(f.filter(3.0, i / 30.0), 3.0, 1e-9);
}

TEST(OneEuroFilter, ReducesJitterWhileStill)
{
  // Head held still: angle 0 with 0.5 degree noise, 30 frames per second
  std::mt19937 rng(42);
  std::normal_distribution<double> noise(0.0, 0.5);
  OneEuroFilter f(1.0, 0.05);
  double sum_in = 0, sum_out = 0;
  for (int i = 0; i < 300; ++i) {
    const double in = noise(rng);
    const double out = f.filter(in, i / 30.0);
    if (i >= 30) { sum_in += in * in; sum_out += out * out; }
  }
  const double rms_in = std::sqrt(sum_in / 270), rms_out = std::sqrt(sum_out / 270);
  EXPECT_LT(rms_out, rms_in * 0.5);
}

TEST(OneEuroFilter, FollowsAFastTurnQuickly)
{
  // Head turns from 0 to 20 degrees at once; after 0.2 s (6 frames)
  // the output must have covered most of the way.
  OneEuroFilter f(1.0, 0.05);
  for (int i = 0; i < 30; ++i) f.filter(0.0, i / 30.0);
  double out = 0;
  for (int i = 30; i < 36; ++i) out = f.filter(20.0, i / 30.0);
  EXPECT_GT(out, 15.0);
}

TEST(OneEuroFilter, IgnoresRepeatedTimestamp)
{
  OneEuroFilter f;
  f.filter(1.0, 1.0);
  EXPECT_DOUBLE_EQ(f.filter(100.0, 1.0), 1.0);
}

TEST(OneEuroFilter, ResetStartsAfresh)
{
  OneEuroFilter f;
  f.filter(1.0, 0.0);
  f.filter(1.0, 0.1);
  f.reset();
  EXPECT_FALSE(f.initialized());
  EXPECT_DOUBLE_EQ(f.filter(9.0, 0.2), 9.0);
}
