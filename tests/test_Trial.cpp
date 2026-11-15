#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "RPiHTConfig.h"
#include "TempDir.h"
#include "Trial.h"

namespace
{
  const char* kConfigJson = R"({ "device": "/dev/hidg0", "port": 18080, "cam_width": 640,
    "cam_height": 480, "cam_framerate": 30.0, "cam_verbose": false })";

  class TrialTest : public ::testing::Test
  {
  protected:
    TempDir dir;
    std::string path = dir.write("USBHT.json", kConfigJson).string();
    std::string marker = path + ".trial";
    RPiHTConfig config{ path };
    using clock = Trial::clock;
    const clock::time_point t0 = clock::now();
  };
}

TEST_F(TrialTest, NoTrialAtFirst)
{
  Trial trial(config, marker);
  EXPECT_FALSE(trial.active());
  EXPECT_EQ(trial.seconds_left(), -1);
  EXPECT_FALSE(trial.expired());
  EXPECT_EQ(trial.failure(), "");
  EXPECT_FALSE(std::filesystem::exists(marker));
}

TEST_F(TrialTest, StartWritesTheLastWorkingSettingsToTheMarker)
{
  Trial trial(config, marker);
  RPiHTConfigData good = config.get_data();
  good.port = 18080;
  trial.start(good);
  EXPECT_TRUE(trial.active());
  ASSERT_TRUE(std::filesystem::exists(marker));
  RPiHTConfigData read;
  ASSERT_TRUE(config.read_file(marker, read));
  EXPECT_EQ(read.port, 18080);
}

TEST_F(TrialTest, CountdownStartsWhenTheProgramRuns)
{
  Trial trial(config, marker, std::chrono::seconds(30));
  trial.start(config.get_data());
  EXPECT_EQ(trial.seconds_left(t0), 30);       // still restarting: full time
  EXPECT_FALSE(trial.expired(t0 + std::chrono::hours(1)));
  trial.started(t0);
  EXPECT_EQ(trial.seconds_left(t0 + std::chrono::seconds(10)), 20);
  EXPECT_FALSE(trial.expired(t0 + std::chrono::seconds(29)));
  EXPECT_TRUE(trial.expired(t0 + std::chrono::seconds(30)));
  EXPECT_EQ(trial.seconds_left(t0 + std::chrono::seconds(40)), 0);
}

TEST_F(TrialTest, KeepEndsTheTrialAndRemovesTheMarker)
{
  Trial trial(config, marker);
  trial.start(config.get_data());
  trial.started(t0);
  trial.keep();
  EXPECT_FALSE(trial.active());
  EXPECT_FALSE(trial.expired(t0 + std::chrono::hours(1)));
  EXPECT_FALSE(std::filesystem::exists(marker));
  EXPECT_EQ(trial.failure(), "");
}

TEST_F(TrialTest, FailGivesBackTheLastWorkingSettingsAndTheReason)
{
  Trial trial(config, marker);
  RPiHTConfigData good = config.get_data();
  good.cam_framerate = 25;
  trial.start(good);
  const RPiHTConfigData back = trial.fail("camera broken");
  EXPECT_FLOAT_EQ(back.cam_framerate, 25);
  EXPECT_FALSE(trial.active());
  EXPECT_EQ(trial.failure(), "camera broken");
  EXPECT_FALSE(std::filesystem::exists(marker));
  trial.dismiss();
  EXPECT_EQ(trial.failure(), "");
}

TEST_F(TrialTest, CancelGivesBackTheLastWorkingSettingsWithoutFailure)
{
  Trial trial(config, marker);
  RPiHTConfigData good = config.get_data();
  good.port = 18081;
  trial.start(good);
  EXPECT_EQ(trial.cancel().port, 18081);
  EXPECT_FALSE(trial.active());
  EXPECT_EQ(trial.failure(), "");
}

TEST_F(TrialTest, StartClearsAnOldFailure)
{
  Trial trial(config, marker);
  trial.start(config.get_data());
  trial.fail("old");
  trial.start(config.get_data());
  EXPECT_EQ(trial.failure(), "");
}

TEST_F(TrialTest, RecoverAfterTheProgramStoppedDuringATrial)
{
  RPiHTConfigData good = config.get_data();
  good.port = 18099;
  {
    Trial trial(config, marker);
    trial.start(good);
  }  // the program "stops" here: the marker stays

  Trial next(config, marker);
  RPiHTConfigData back;
  ASSERT_TRUE(next.recover(back));
  EXPECT_EQ(back.port, 18099);
  EXPECT_NE(next.failure(), "");
  EXPECT_FALSE(next.active());
  EXPECT_FALSE(std::filesystem::exists(marker));
}

TEST_F(TrialTest, RecoverWithoutMarkerDoesNothing)
{
  Trial trial(config, marker);
  RPiHTConfigData back;
  EXPECT_FALSE(trial.recover(back));
  EXPECT_EQ(trial.failure(), "");
}
