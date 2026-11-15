#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <unistd.h>

#include "RPiHTConfig.h"
#include "TempDir.h"

namespace
{
  const char* kConfigJson = R"({
   "cam_framerate": 80.0,
   "cam_height": 480,
   "cam_verbose": false,
   "cam_width": 640,
   "device": "/dev/hidg0",
   "ip": "0.0.0.0",
   "port": 18080
})";

  class RPiHTConfigTest : public ::testing::Test
  {
  protected:
    TempDir dir;
    std::string path = dir.write("USBHT.json", kConfigJson).string();
  };
}

TEST_F(RPiHTConfigTest, LoadsAllFieldsFromFile)
{
  RPiHTConfig config(path);
  const auto& d = config.get_data();
  EXPECT_EQ(d.device_path, "/dev/hidg0");
  EXPECT_EQ(d.ip, "0.0.0.0");
  EXPECT_EQ(d.port, 18080);
  EXPECT_EQ(d.cam_width, 640u);
  EXPECT_EQ(d.cam_height, 480u);
  EXPECT_FLOAT_EQ(d.cam_framerate, 80.0f);
  EXPECT_FALSE(d.cam_verbose);
}

TEST_F(RPiHTConfigTest, FreshlyLoadedConfigIsSavedAndActive)
{
  RPiHTConfig config(path);
  EXPECT_TRUE(config.get_data().is_saved);
  EXPECT_TRUE(config.get_data().is_active);
}

TEST_F(RPiHTConfigTest, IpIsOptional)
{
  auto p = dir.write("no_ip.json", R"({ "device": "/dev/hidg0", "port": 1, "cam_width": 1,
    "cam_height": 1, "cam_framerate": 1.0, "cam_verbose": true })").string();
  RPiHTConfig config(p);
  EXPECT_EQ(config.get_ip(), "");
  EXPECT_TRUE(config.get_data().cam_verbose);
}

TEST_F(RPiHTConfigTest, GettersReturnLoadedValues)
{
  RPiHTConfig config(path);
  EXPECT_EQ(config.get_device_path(), "/dev/hidg0");
  EXPECT_EQ(config.get_ip(), "0.0.0.0");
}

TEST_F(RPiHTConfigTest, EqualityIgnoresSavedAndActiveFlags)
{
  RPiHTConfig config(path);
  RPiHTConfigData copy = config.get_data();
  copy.is_saved = !copy.is_saved;
  copy.is_active = !copy.is_active;
  EXPECT_TRUE(copy == config.get_data());

  copy.port = 1;
  EXPECT_FALSE(copy == config.get_data());
}

TEST_F(RPiHTConfigTest, SetDataWithSameValuesKeepsFlags)
{
  RPiHTConfig config(path);
  RPiHTConfigData same = config.get_data();
  config.set_data(same);
  EXPECT_TRUE(config.get_data().is_saved);
  EXPECT_TRUE(config.get_data().is_active);
}

TEST_F(RPiHTConfigTest, SetDataWithChangedValuesUpdatesAndClearsFlags)
{
  RPiHTConfig config(path);
  RPiHTConfigData changed = config.get_data();
  changed.cam_framerate = 60.0f;
  changed.cam_verbose = true;
  config.set_data(changed);

  EXPECT_FLOAT_EQ(config.get_data().cam_framerate, 60.0f);
  EXPECT_TRUE(config.get_data().cam_verbose);
  EXPECT_FALSE(config.get_data().is_saved);
  EXPECT_FALSE(config.get_data().is_active);
}

TEST_F(RPiHTConfigTest, SettersUpdateAndClearFlags)
{
  RPiHTConfig config(path);
  config.set_device_path("/dev/hidg1");
  EXPECT_EQ(config.get_device_path(), "/dev/hidg1");
  EXPECT_FALSE(config.get_data().is_saved);
  EXPECT_FALSE(config.get_data().is_active);

  RPiHTConfig config2(path);
  config2.set_ip("127.0.0.1");
  EXPECT_EQ(config2.get_ip(), "127.0.0.1");
  EXPECT_FALSE(config2.get_data().is_saved);
  EXPECT_FALSE(config2.get_data().is_active);
}

TEST_F(RPiHTConfigTest, SetActive)
{
  RPiHTConfig config(path);
  config.set_active(false);
  EXPECT_FALSE(config.get_data().is_active);
  config.set_active(true);
  EXPECT_TRUE(config.get_data().is_active);
}

TEST_F(RPiHTConfigTest, SaveWritesValuesThatLoadBack)
{
  {
    RPiHTConfig config(path);
    RPiHTConfigData changed = config.get_data();
    changed.port = 8080;
    changed.cam_width = 320;
    changed.cam_height = 240;
    changed.cam_framerate = 30.0f;
    changed.cam_verbose = true;
    changed.ip = "127.0.0.1";
    config.set_data(changed);
    config.save();
    EXPECT_TRUE(config.get_data().is_saved);
  }

  RPiHTConfig reloaded(path);
  const auto& d = reloaded.get_data();
  EXPECT_EQ(d.port, 8080);
  EXPECT_EQ(d.cam_width, 320u);
  EXPECT_EQ(d.cam_height, 240u);
  EXPECT_FLOAT_EQ(d.cam_framerate, 30.0f);
  EXPECT_TRUE(d.cam_verbose);
  EXPECT_EQ(d.ip, "127.0.0.1");
  EXPECT_EQ(d.device_path, "/dev/hidg0");
}

TEST_F(RPiHTConfigTest, SavedFileRecordsSavedTrue)
{
  RPiHTConfig config(path);
  RPiHTConfigData changed = config.get_data();
  changed.port = 8080;
  config.set_data(changed);
  config.save();

  const std::string text = read_file(path);
  EXPECT_NE(text.find("\"saved\": true"), std::string::npos) << text;
}

TEST_F(RPiHTConfigTest, SaveToReadOnlyFileClearsSavedFlag)
{
  if (::geteuid() == 0)
    GTEST_SKIP() << "root can write read-only files";

  RPiHTConfig config(path);
  RPiHTConfigData changed = config.get_data();
  changed.port = 8080;
  config.set_data(changed);

  std::filesystem::permissions(path, std::filesystem::perms::owner_read,
    std::filesystem::perm_options::replace);
  config.save();
  std::filesystem::permissions(path, std::filesystem::perms::owner_all,
    std::filesystem::perm_options::replace);

  EXPECT_FALSE(config.get_data().is_saved);
  EXPECT_NE(read_file(path).find("18080"), std::string::npos) << "file must be unchanged";
}

// A missing or broken config file is reported on stderr, not thrown.
TEST_F(RPiHTConfigTest, MissingFileDoesNotThrow)
{
  std::string missing = dir.file("does_not_exist.json").string();
  EXPECT_NO_THROW({ RPiHTConfig config(missing); });
}

TEST_F(RPiHTConfigTest, MalformedFileDoesNotThrow)
{
  auto bad = dir.write("bad.json", "{ this is not json").string();
  EXPECT_NO_THROW({ RPiHTConfig config(bad); });
}

// Head tracking settings: optional in the file, with defaults

TEST_F(RPiHTConfigTest, TrackingSettingsDefaultWhenMissing)
{
  RPiHTConfig config(path);  // kConfigJson has no track_* keys
  const auto& d = config.get_data();
  EXPECT_DOUBLE_EQ(d.track_range_x, 20.0);
  EXPECT_DOUBLE_EQ(d.track_range_y, 10.0);
  EXPECT_DOUBLE_EQ(d.track_deadzone, 1.5);
  EXPECT_DOUBLE_EQ(d.track_curve, 1.0);
  EXPECT_DOUBLE_EQ(d.track_smoothing, 1.0);
  EXPECT_DOUBLE_EQ(d.track_speed, 0.05);
  EXPECT_FALSE(d.track_invert_x);
  EXPECT_FALSE(d.track_invert_y);
  EXPECT_EQ(d.track_detect_width, 256);
}

TEST_F(RPiHTConfigTest, TrackingSettingsLoadFromFile)
{
  auto p = dir.write("tracking.json", R"({ "device": "/dev/hidg0", "port": 1, "cam_width": 1,
    "cam_height": 1, "cam_framerate": 1.0, "cam_verbose": false,
    "track_range_x": 15, "track_range_y": 12.5, "track_deadzone": 2.0, "track_curve": 1.5,
    "track_smoothing": 0.5, "track_speed": 0.1, "track_invert_x": true, "track_invert_y": false,
    "track_detect_width": 320 })").string();
  RPiHTConfig config(p);
  const auto& d = config.get_data();
  EXPECT_EQ(d.track_detect_width, 320);
  EXPECT_DOUBLE_EQ(d.track_range_x, 15.0);  // whole number in the file
  EXPECT_DOUBLE_EQ(d.track_range_y, 12.5);
  EXPECT_DOUBLE_EQ(d.track_deadzone, 2.0);
  EXPECT_DOUBLE_EQ(d.track_curve, 1.5);
  EXPECT_DOUBLE_EQ(d.track_smoothing, 0.5);
  EXPECT_DOUBLE_EQ(d.track_speed, 0.1);
  EXPECT_TRUE(d.track_invert_x);
  EXPECT_FALSE(d.track_invert_y);
}

TEST_F(RPiHTConfigTest, ChangedTrackingSettingCountsAsChange)
{
  RPiHTConfig config(path);
  RPiHTConfigData changed = config.get_data();
  changed.track_range_x = 30.0;
  EXPECT_FALSE(changed == config.get_data());
  config.set_data(changed);
  EXPECT_DOUBLE_EQ(config.get_data().track_range_x, 30.0);
  EXPECT_FALSE(config.get_data().is_saved);

  RPiHTConfigData inverted = config.get_data();
  inverted.track_invert_y = true;
  EXPECT_FALSE(inverted == config.get_data());

  RPiHTConfigData resized = config.get_data();
  resized.track_detect_width = 160;
  EXPECT_FALSE(resized == config.get_data());
}

TEST_F(RPiHTConfigTest, SaveWritesTrackingSettingsThatLoadBack)
{
  {
    RPiHTConfig config(path);
    RPiHTConfigData changed = config.get_data();
    changed.track_range_x = 25.0;
    changed.track_range_y = 18.0;
    changed.track_deadzone = 0.5;
    changed.track_curve = 2.0;
    changed.track_smoothing = 0.8;
    changed.track_speed = 0.2;
    changed.track_invert_x = false;
    changed.track_invert_y = true;
    changed.track_detect_width = 192;
    config.set_data(changed);
    config.save();
  }

  RPiHTConfig reloaded(path);
  const auto& d = reloaded.get_data();
  EXPECT_DOUBLE_EQ(d.track_range_x, 25.0);
  EXPECT_DOUBLE_EQ(d.track_range_y, 18.0);
  EXPECT_DOUBLE_EQ(d.track_deadzone, 0.5);
  EXPECT_DOUBLE_EQ(d.track_curve, 2.0);
  EXPECT_DOUBLE_EQ(d.track_smoothing, 0.8);
  EXPECT_DOUBLE_EQ(d.track_speed, 0.2);
  EXPECT_FALSE(d.track_invert_x);
  EXPECT_TRUE(d.track_invert_y);
  EXPECT_EQ(d.track_detect_width, 192);
  EXPECT_EQ(d.port, 18080);  // other values unchanged
}
