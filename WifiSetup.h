#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Wi-Fi of the Pi, through NetworkManager (nmcli): which network it is on,
// which networks are nearby, joining one and forgetting one.
// The hotspot itself (the tracker's own Wi-Fi when it is on no network) is
// opened by wifi/headtracker-wifi.sh; this class only knows its connection name.

struct WifiNetwork
{
  std::string ssid;
  int signal = 0;         // 0..100
  std::string security;   // as nmcli shows it: "" (open), "WPA2", "WPA1 WPA2", "WPA2 WPA3", "WPA3", "WEP", "WPA2 802.1X", ...
  bool open() const { return security.empty() || security == "--"; }
};

struct WifiStatus
{
  bool available = false;  // nmcli works and there is a Wi-Fi device
  std::string mode;        // "client" (on a network), "hotspot" (own Wi-Fi), "connecting" or "off"
  std::string ssid;        // network name (in hotspot mode: the hotspot's own name)
  std::string address;     // IPv4 address on the Wi-Fi, without "/24"
};

struct WifiJoin
{
  std::string ssid;     // "" if nothing was joined since the program started
  bool busy = false;    // still trying
  bool ok = false;
  std::string message;  // why it failed
  int age_s = 0;        // seconds since it ended
};

namespace wifi
{
  // Fields of one line of "nmcli -t" output: separated by ':', where "\:" is a
  // colon inside a field and "\\" a backslash
  std::vector<std::string> split_terse(const std::string& line);
  // Networks from "nmcli -t -f SSID,SIGNAL,SECURITY dev wifi list": hidden
  // ones (no name) left out, each name once with its strongest signal, strongest first
  std::vector<WifiNetwork> parse_networks(const std::string& output);
  // NetworkManager key management for a security text: "" (open), "wpa-psk",
  // "sae" (WPA3 only); "unsupported" for WEP and enterprise (802.1X) networks
  std::string key_mgmt_for(const std::string& security);
  // The values of "nmcli -g field1,field2,... connection show": nmcli prints
  // one value per line (a ':' in a value is escaped as "\:")
  std::vector<std::string> split_values(const std::string& output);
  // A short explanation for a failed "nmcli connection up"
  std::string join_failure(const std::string& nmcli_output);
  // true if a browser asked for this tracker by a name it has ('host' is the
  // request's Host header, 'name' the host name): an IP address, the host name
  // (also "dacia.local" or "dacia.fritz.box") or localhost. Any other name comes
  // from a phone on the hotspot checking for internet ("connectivitycheck.gstatic.com"),
  // which then gets the setup page.
  bool own_host_name(std::string host, const std::string& name);
}

class WifiSetup
{
public:
  // 'hotspot': name of the hotspot connection in NetworkManager;
  // 'scan_file': networks seen before the hotspot was opened (written by headtracker-wifi.sh)
  explicit WifiSetup(std::string device = "wlan0",
                     std::string hotspot = "headtracker-hotspot",
                     std::string scan_file = "/run/headtracker-wifi/networks");
  ~WifiSetup();
  WifiSetup(const WifiSetup&) = delete;
  WifiSetup& operator=(const WifiSetup&) = delete;

  WifiStatus status() const;
  // Nearby networks. In hotspot mode the Wi-Fi cannot search, so these are the
  // ones seen just before the hotspot was opened.
  std::vector<WifiNetwork> networks();
  // Starts joining a network in the background (see last_join()). The Wi-Fi
  // leaves its current network or the hotspot for this. If it fails, the new
  // network is forgotten again and a hotspot that was open is opened again.
  // false if a join is already running.
  bool join(const std::string& ssid, const std::string& password);
  WifiJoin last_join() const;
  // Forgets the network the Pi is on now; without another known network the
  // hotspot opens shortly after. false if it is on no network.
  bool forget();

private:
  void do_join(std::string ssid, std::string password, bool from_hotspot);
  std::string security_of(const std::string& ssid) const;
  std::vector<std::string> uuids_for_ssid(const std::string& ssid) const;

  const std::string device_;
  const std::string hotspot_;
  const std::string scan_file_;
  mutable std::mutex mtx_;
  std::vector<WifiNetwork> seen_;  // last list given to the page
  WifiJoin join_;
  std::chrono::steady_clock::time_point join_end_;
  std::thread worker_;
};
