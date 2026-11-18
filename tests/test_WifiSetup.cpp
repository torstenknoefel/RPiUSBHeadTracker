#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Command.h"
#include "WifiSetup.h"

using wifi::split_terse;

TEST(WifiSplitTerse, SplitsAtColons)
{
  EXPECT_EQ(split_terse("ParkNet:80:WPA2:*"), (std::vector<std::string>{ "ParkNet", "80", "WPA2", "*" }));
}

TEST(WifiSplitTerse, KeepsEscapedColonsAndBackslashes)
{
  EXPECT_EQ(split_terse(R"(Cafe\: Free\\Wifi:55:)"), (std::vector<std::string>{ R"(Cafe: Free\Wifi)", "55", "" }));
}

TEST(WifiSplitTerse, EmptyFirstFieldForHiddenNetworks)
{
  EXPECT_EQ(split_terse(":100:WPA2: "), (std::vector<std::string>{ "", "100", "WPA2", " " }));
}

TEST(WifiParseNetworks, LeavesOutHiddenOnesAndSortsByStrength)
{
  const auto list = wifi::parse_networks(
    ":100:WPA2: \n"
    "ParkNet:80:WPA2:*\n"
    "Guest:30:--: \n"
    "vodafoneBD5B2A:52:WPA2 WPA3: \n");
  ASSERT_EQ(list.size(), 3u);
  EXPECT_EQ(list[0].ssid, "ParkNet");
  EXPECT_EQ(list[0].signal, 80);
  EXPECT_EQ(list[1].ssid, "vodafoneBD5B2A");
  EXPECT_EQ(list[1].security, "WPA2 WPA3");
  EXPECT_EQ(list[2].ssid, "Guest");
  EXPECT_TRUE(list[2].open());
  EXPECT_FALSE(list[0].open());
}

TEST(WifiParseNetworks, EachNameOnceWithItsStrongestSignal)
{
  const auto list = wifi::parse_networks("Home:40:WPA2:\nHome:75:WPA2:\nHome:60:WPA2:\n");
  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].signal, 75);
}

TEST(WifiParseNetworks, EmptyOutputGivesNoNetworks)
{
  EXPECT_TRUE(wifi::parse_networks("").empty());
}

TEST(WifiKeyMgmt, FromSecurityText)
{
  EXPECT_EQ(wifi::key_mgmt_for(""), "");
  EXPECT_EQ(wifi::key_mgmt_for("--"), "");
  EXPECT_EQ(wifi::key_mgmt_for("WPA2"), "wpa-psk");
  EXPECT_EQ(wifi::key_mgmt_for("WPA1 WPA2"), "wpa-psk");
  EXPECT_EQ(wifi::key_mgmt_for("WPA2 WPA3"), "wpa-psk");  // transition mode: WPA2 works
  EXPECT_EQ(wifi::key_mgmt_for("WPA3"), "sae");
  EXPECT_EQ(wifi::key_mgmt_for("WEP"), "unsupported");
  EXPECT_EQ(wifi::key_mgmt_for("WPA2 802.1X"), "unsupported");
}

TEST(WifiJoinFailure, ExplainsCommonErrors)
{
  EXPECT_EQ(wifi::join_failure("Error: Connection activation failed: Secrets were required, but not provided."),
            "The password was not accepted.");
  EXPECT_EQ(wifi::join_failure("Error: No network with SSID 'Home' found."),
            "The network was not found. Is the Pi close enough to the router?");
  EXPECT_EQ(wifi::join_failure("Error: Timeout expired (45 seconds)"), "The network did not answer in time.");
  EXPECT_EQ(wifi::join_failure("Warning: x\nError: Something else.\n"), "Something else.");
  EXPECT_EQ(wifi::join_failure(""), "The network could not be joined.");
}

TEST(WifiOwnHostName, AddressesAndOwnNamesAreOwn)
{
  EXPECT_TRUE(wifi::own_host_name("10.42.0.1", "dacia"));
  EXPECT_TRUE(wifi::own_host_name("10.1.1.152:18080", "dacia"));
  EXPECT_TRUE(wifi::own_host_name("[fe80::1]:18080", "dacia"));
  EXPECT_TRUE(wifi::own_host_name("dacia:18080", "dacia"));
  EXPECT_TRUE(wifi::own_host_name("Dacia.local", "dacia"));
  EXPECT_TRUE(wifi::own_host_name("dacia.fritz.box:18080", "dacia"));
  EXPECT_TRUE(wifi::own_host_name("localhost:18080", "dacia"));
  EXPECT_TRUE(wifi::own_host_name("", "dacia"));
}

TEST(WifiOwnHostName, OtherNamesAreNotOwn)
{
  EXPECT_FALSE(wifi::own_host_name("connectivitycheck.gstatic.com", "dacia"));
  EXPECT_FALSE(wifi::own_host_name("captive.apple.com", "dacia"));
  EXPECT_FALSE(wifi::own_host_name("www.msftconnecttest.com", "dacia"));
  EXPECT_FALSE(wifi::own_host_name("daciaxyz.com", "dacia"));
  EXPECT_FALSE(wifi::own_host_name("example.com", ""));
}

TEST(RunCommand, GivesOutputAndExitCode)
{
  std::string out;
  EXPECT_EQ(run_command({ "echo", "hello" }, out), 0);
  EXPECT_EQ(out, "hello\n");
  EXPECT_EQ(run_command({ "sh", "-c", "echo oops >&2; exit 3" }, out), 3);
  EXPECT_EQ(out, "oops\n");
}

TEST(RunCommand, PassesArgumentsWithoutAShell)
{
  std::string out;
  EXPECT_EQ(run_command({ "echo", "$(id) ; rm -rf x" }, out), 0);
  EXPECT_EQ(out, "$(id) ; rm -rf x\n");
}

TEST(RunCommand, MissingProgramAndTimeout)
{
  std::string out;
  EXPECT_EQ(run_command({ "no-such-program-here" }, out), 127);
  EXPECT_EQ(run_command({ "sleep", "5" }, out, 1), -1);
  EXPECT_EQ(run_command({}, out), -1);
}
