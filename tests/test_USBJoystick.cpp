#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "USBJoystick.h"
#include "TempDir.h"

// The joystick writes HID reports to a device file (/dev/hidg0 on the Pi).
// Here it writes to a normal file so the reports can be checked byte by byte.

namespace
{
  std::vector<uint8_t> bytes_of(const std::filesystem::path& p)
  {
    std::string s = read_file(p);
    return std::vector<uint8_t>(s.begin(), s.end());
  }
}

TEST(USBJoystick, ReportIsSixBytes_XY_Then32ButtonsLittleEndian)
{
  TempDir dir;
  auto dev = dir.file("hidg0");
  {
    USBJoystick joystick(dev.string());
    EXPECT_TRUE(joystick.update(0, 0, 10, -20, 0x04030201u, 0));
  }
  EXPECT_EQ(bytes_of(dev), (std::vector<uint8_t>{ 10, 0xEC, 0x01, 0x02, 0x03, 0x04 }));
}

TEST(USBJoystick, EachUpdateWritesOneReport)
{
  TempDir dir;
  auto dev = dir.file("hidg0");
  {
    USBJoystick joystick(dev.string());
    joystick.update(0, 0, 1, 2, 0, 0);
    joystick.update(0, 0, 3, 4, 0xFFFFFFFFu, 0);
  }
  EXPECT_EQ(bytes_of(dev), (std::vector<uint8_t>{ 1, 2, 0, 0, 0, 0, 3, 4, 0xFF, 0xFF, 0xFF, 0xFF }));
}

TEST(USBJoystick, AxisLimitsAreSentUnchanged)
{
  TempDir dir;
  auto dev = dir.file("hidg0");
  {
    USBJoystick joystick(dev.string());
    joystick.update(0, 0, JX_MIN_ABS, JY_MAX_ABS, 0, 0);
  }
  auto b = bytes_of(dev);
  ASSERT_EQ(b.size(), 6u);
  EXPECT_EQ(static_cast<int8_t>(b[0]), -127);
  EXPECT_EQ(static_cast<int8_t>(b[1]), 127);
}

// Throttle, rudder and hat are not part of the current HID report descriptor.
TEST(USBJoystick, ThrottleRudderAndHatAreNotSent)
{
  TempDir dir;
  auto dev = dir.file("hidg0");
  {
    USBJoystick joystick(dev.string());
    joystick.update(100, -100, 0, 0, 0, JOY_HAT_LEFT);
  }
  EXPECT_EQ(bytes_of(dev), (std::vector<uint8_t>{ 0, 0, 0, 0, 0, 0 }));
}

TEST(USBJoystick, UpdateWithoutArgumentsResendsLastState)
{
  TempDir dir;
  auto dev = dir.file("hidg0");
  {
    USBJoystick joystick(dev.string());
    joystick.update(0, 0, 5, 6, 0x80u, 0);
    EXPECT_TRUE(joystick.update());
  }
  EXPECT_EQ(bytes_of(dev), (std::vector<uint8_t>{ 5, 6, 0x80, 0, 0, 0, 5, 6, 0x80, 0, 0, 0 }));
}
