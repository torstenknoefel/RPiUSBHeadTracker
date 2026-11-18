#include "WifiSetup.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

#include "Command.h"

namespace wifi
{
  std::vector<std::string> split_terse(const std::string& line)
  {
    std::vector<std::string> fields(1);
    for (size_t i = 0; i < line.size(); ++i) {
      const char c = line[i];
      if (c == '\\' && i + 1 < line.size()) {
        fields.back() += line[++i];
      }
      else if (c == ':') {
        fields.emplace_back();
      }
      else if (c != '\r') {
        fields.back() += c;
      }
    }
    return fields;
  }

  std::vector<WifiNetwork> parse_networks(const std::string& output)
  {
    std::map<std::string, WifiNetwork> by_name;
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
      const auto f = split_terse(line);
      if (f.size() < 3 || f[0].empty())
        continue;
      WifiNetwork n;
      n.ssid = f[0];
      try { n.signal = std::stoi(f[1]); }
      catch (const std::exception&) { n.signal = 0; }
      n.security = f[2] == "--" ? "" : f[2];
      auto it = by_name.find(n.ssid);
      if (it == by_name.end() || it->second.signal < n.signal)
        by_name[n.ssid] = n;
    }
    std::vector<WifiNetwork> list;
    for (auto& [name, n] : by_name)
      list.push_back(n);
    std::stable_sort(list.begin(), list.end(),
      [](const WifiNetwork& a, const WifiNetwork& b) { return a.signal > b.signal; });
    return list;
  }

  std::vector<std::string> split_values(const std::string& output)
  {
    std::vector<std::string> values;
    std::istringstream lines(output);
    for (std::string l; std::getline(lines, l);) {
      const auto f = split_terse(l);
      std::string v = f[0];
      for (size_t i = 1; i < f.size(); ++i)
        v += ":" + f[i];  // an unescaped ':' (should not happen) stays in the value
      values.push_back(v);
    }
    return values;
  }

  std::string key_mgmt_for(const std::string& security)
  {
    if (security.empty() || security == "--")
      return "";
    if (security.find("802.1X") != std::string::npos || security.find("WEP") != std::string::npos)
      return "unsupported";
    if (security.find("WPA3") != std::string::npos && security.find("WPA2") == std::string::npos
        && security.find("WPA1") == std::string::npos)
      return "sae";
    return "wpa-psk";
  }

  std::string join_failure(const std::string& out)
  {
    if (out.find("Secrets were required") != std::string::npos
        || out.find("802-11-wireless-security.psk") != std::string::npos
        || out.find("supplicant") != std::string::npos)
      return "The password was not accepted.";
    if (out.find("No network with SSID") != std::string::npos
        || out.find("not found") != std::string::npos)
      return "The network was not found. Is the Pi close enough to the router?";
    if (out.find("Timeout") != std::string::npos || out.find("timed out") != std::string::npos)
      return "The network did not answer in time.";
    // Last line of nmcli's message, without "Error: "
    std::string last;
    std::istringstream lines(out);
    for (std::string l; std::getline(lines, l);)
      if (!l.empty())
        last = l;
    if (last.rfind("Error: ", 0) == 0)
      last.erase(0, 7);
    return last.empty() ? "The network could not be joined." : last;
  }

  bool own_host_name(std::string host, const std::string& name)
  {
    if (host.empty() || host[0] == '[')
      return true;  // no name, or an IPv6 address
    host.erase(std::min(host.find(':'), host.size()));
    std::transform(host.begin(), host.end(), host.begin(), ::tolower);
    if (host.find_first_not_of("0123456789.") == std::string::npos || host == "localhost")
      return true;
    std::string own = name;
    std::transform(own.begin(), own.end(), own.begin(), ::tolower);
    return !own.empty() && (host == own || host.rfind(own + ".", 0) == 0);
  }
}

namespace
{
  // "key:value" lines of "nmcli -t -f ... device show" as a map
  std::map<std::string, std::string> fields_of(const std::string& output)
  {
    std::map<std::string, std::string> m;
    std::istringstream lines(output);
    for (std::string l; std::getline(lines, l);) {
      const auto f = wifi::split_terse(l);
      if (f.size() >= 2 && !m.count(f[0]))
        m[f[0]] = f[1];
    }
    return m;
  }

  std::string first_value(const std::string& output)
  {
    const auto f = wifi::split_terse(output.substr(0, output.find('\n')));
    return f.empty() ? std::string() : f[0];
  }
}

WifiSetup::WifiSetup(std::string device, std::string hotspot, std::string scan_file)
  : device_(std::move(device)), hotspot_(std::move(hotspot)), scan_file_(std::move(scan_file))
{
}

WifiSetup::~WifiSetup()
{
  if (worker_.joinable())
    worker_.join();
}

WifiStatus WifiSetup::status() const
{
  WifiStatus s;
  std::string out;
  if (run_command({ "nmcli", "-t", "-f", "GENERAL.STATE,GENERAL.CONNECTION,GENERAL.CON-UUID,IP4.ADDRESS",
                    "device", "show", device_ }, out) != 0)
    return s;
  s.available = true;
  auto f = fields_of(out);
  const int state = std::atoi(f["GENERAL.STATE"].c_str());  // "100 (connected)"
  if (state == 100) {
    s.mode = f["GENERAL.CONNECTION"] == hotspot_ ? "hotspot" : "client";
    std::string ssid;
    if (run_command({ "nmcli", "-t", "-g", "802-11-wireless.ssid", "connection", "show",
                      "uuid", f["GENERAL.CON-UUID"] }, ssid) == 0)
      s.ssid = first_value(ssid);
  }
  else if (state > 30 && state < 100) {
    s.mode = "connecting";
  }
  else {
    s.mode = "off";
  }
  const std::string a = f["IP4.ADDRESS[1]"];
  s.address = a.substr(0, a.find('/'));
  return s;
}

std::vector<WifiNetwork> WifiSetup::networks()
{
  std::string out;
  if (status().mode == "hotspot") {
    std::ifstream file(scan_file_);
    std::ostringstream text;
    text << file.rdbuf();
    out = text.str();
  }
  else if (run_command({ "nmcli", "-t", "-f", "SSID,SIGNAL,SECURITY", "device", "wifi", "list",
                         "ifname", device_ }, out, 20) != 0) {
    out.clear();
  }
  auto list = wifi::parse_networks(out);
  std::lock_guard<std::mutex> lock(mtx_);
  seen_ = list;
  return list;
}

bool WifiSetup::join(const std::string& ssid, const std::string& password)
{
  const bool from_hotspot = status().mode == "hotspot";
  std::lock_guard<std::mutex> lock(mtx_);
  if (join_.busy || ssid.empty())
    return false;
  if (worker_.joinable())
    worker_.join();  // the last one has finished (it is not busy)
  join_ = WifiJoin{ ssid, true, false, "" };
  worker_ = std::thread(&WifiSetup::do_join, this, ssid, password, from_hotspot);
  return true;
}

WifiJoin WifiSetup::last_join() const
{
  std::lock_guard<std::mutex> lock(mtx_);
  WifiJoin j = join_;
  if (!j.busy && !j.ssid.empty())
    j.age_s = int(std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - join_end_).count());
  return j;
}

bool WifiSetup::forget()
{
  std::string out;
  if (run_command({ "nmcli", "-t", "-f", "GENERAL.STATE,GENERAL.CONNECTION,GENERAL.CON-UUID",
                    "device", "show", device_ }, out) != 0)
    return false;
  auto f = fields_of(out);
  if (std::atoi(f["GENERAL.STATE"].c_str()) != 100 || f["GENERAL.CONNECTION"] == hotspot_)
    return false;
  return run_command({ "nmcli", "connection", "delete", "uuid", f["GENERAL.CON-UUID"] }, out) == 0;
}

// Security of a network from the last list; "?" if it was not in it (a hidden network)
std::string WifiSetup::security_of(const std::string& ssid) const
{
  std::lock_guard<std::mutex> lock(mtx_);
  for (const auto& n : seen_)
    if (n.ssid == ssid)
      return n.security;
  return "?";
}

// Saved Wi-Fi connections (not the hotspot) for a network name
std::vector<std::string> WifiSetup::uuids_for_ssid(const std::string& ssid) const
{
  std::vector<std::string> list;
  std::string out;
  if (run_command({ "nmcli", "-t", "-f", "UUID,TYPE", "connection", "show" }, out) != 0)
    return list;
  std::istringstream lines(out);
  for (std::string l; std::getline(lines, l);) {
    const auto f = wifi::split_terse(l);
    if (f.size() < 2 || f[1] != "802-11-wireless")
      continue;
    std::string v;
    if (run_command({ "nmcli", "-t", "-g", "802-11-wireless.ssid,802-11-wireless.mode",
                      "connection", "show", "uuid", f[0] }, v) != 0)
      continue;
    const auto g = wifi::split_values(v);
    if (g.size() >= 2 && g[0] == ssid && g[1] != "ap")
      list.push_back(f[0]);
  }
  return list;
}

void WifiSetup::do_join(std::string ssid, std::string password, bool from_hotspot)
{
  auto finish = [this](bool ok, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx_);
    join_.busy = false;
    join_.ok = ok;
    join_.message = message;
    join_end_ = std::chrono::steady_clock::now();
  };

  const std::string security = security_of(ssid);
  const bool hidden = security == "?";
  const std::string key_mgmt = hidden ? (password.empty() ? "" : "wpa-psk") : wifi::key_mgmt_for(security);
  if (key_mgmt == "unsupported") {
    finish(false, "This network needs a user name (company network) or uses the old WEP security. The tracker cannot join it.");
    return;
  }
  if (!key_mgmt.empty() && (password.size() < 8 || password.size() > 64)) {
    finish(false, "Wi-Fi passwords have 8 to 63 characters.");
    return;
  }

  // A new connection, so a working one for the same network is only replaced
  // once the new one works
  std::vector<std::string> add = { "nmcli", "connection", "add", "type", "wifi", "ifname", device_,
                                   "con-name", ssid, "ssid", ssid, "connection.autoconnect", "yes" };
  if (hidden)
    add.insert(add.end(), { "802-11-wireless.hidden", "yes" });
  if (!key_mgmt.empty())
    add.insert(add.end(), { "wifi-sec.key-mgmt", key_mgmt, "wifi-sec.psk", password });
  std::string out;
  const int added = run_command(add, out);
  std::smatch m;
  if (added != 0 || !std::regex_search(out, m, std::regex("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"))) {
    finish(false, wifi::join_failure(out));
    return;
  }
  const std::string uuid = m.str();
  const auto old = uuids_for_ssid(ssid);

  if (run_command({ "nmcli", "--wait", "45", "connection", "up", "uuid", uuid }, out, 60) == 0) {
    std::string ignored;
    for (const auto& u : old)
      if (u != uuid)
        run_command({ "nmcli", "connection", "delete", "uuid", u }, ignored);
    finish(true, "");
    return;
  }
  const std::string why = wifi::join_failure(out);
  std::string ignored;
  run_command({ "nmcli", "connection", "delete", "uuid", uuid }, ignored);
  if (from_hotspot)
    run_command({ "nmcli", "--wait", "20", "connection", "up", "id", hotspot_ }, ignored, 30);
  finish(false, why);
}
