#include <gtest/gtest.h>

#include "TrackerSettings.h"

namespace {

RPiHTConfigData config()
{
  RPiHTConfigData d{};
  d.cam_width = 640;
  d.cam_height = 480;
  return d;  // head tracking values: the defaults from RPiHTConfig.h
}

}  // namespace

TEST(TrackerSettings, TakesTheConfigValues)
{
  RPiHTConfigData d = config();
  d.track_range_x = 25;
  d.track_range_y = 12;
  d.track_deadzone = 2;
  d.track_curve = 1.5;
  d.track_smoothing = 0.7;
  d.track_speed = 0.1;
  d.track_invert_y = true;
  d.track_detect_width = 192;
  const TrackerSettings s = tracker_settings(d);
  EXPECT_DOUBLE_EQ(s.x.range, 25);
  EXPECT_DOUBLE_EQ(s.y.range, 12);
  EXPECT_DOUBLE_EQ(s.x.deadzone, 2);
  EXPECT_DOUBLE_EQ(s.y.deadzone, 2);
  EXPECT_DOUBLE_EQ(s.x.curve, 1.5);
  EXPECT_DOUBLE_EQ(s.filter_min_cutoff, 0.7);
  EXPECT_DOUBLE_EQ(s.filter_beta, 0.1);
  EXPECT_FALSE(s.x.invert);
  EXPECT_TRUE(s.y.invert);
  EXPECT_EQ(s.detect_size, cv::Size(192, 144));
  EXPECT_EQ(s.model, FaceModel::YuNet);
}

TEST(TrackerSettings, FaceModel)
{
  RPiHTConfigData d = config();
  d.track_model = "neuralnet";
  EXPECT_EQ(tracker_settings(d).model, FaceModel::NeuralNet);
  d.track_model = "yunet";
  EXPECT_EQ(tracker_settings(d).model, FaceModel::YuNet);
  d.track_model = "something else";  // unknown: the default
  EXPECT_EQ(tracker_settings(d).model, FaceModel::YuNet);
}

TEST(TrackerSettings, OddValuesAreKeptWithinLimits)
{
  RPiHTConfigData d = config();
  d.track_range_x = 0;
  d.track_range_y = -5;
  d.track_deadzone = -1;
  d.track_curve = 0;
  d.track_smoothing = 0;
  d.track_speed = -1;
  const TrackerSettings s = tracker_settings(d);
  EXPECT_DOUBLE_EQ(s.x.range, 1);
  EXPECT_DOUBLE_EQ(s.y.range, 1);
  EXPECT_DOUBLE_EQ(s.x.deadzone, 0);
  EXPECT_DOUBLE_EQ(s.x.curve, 0.1);
  EXPECT_DOUBLE_EQ(s.filter_min_cutoff, 0.05);
  EXPECT_DOUBLE_EQ(s.filter_beta, 0);
}
