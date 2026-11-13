#pragma once
#include <string>
#include <memory>



struct RPiHTConfigData
{
  std::string device_path;
  std::string ip;
  int port;
  uint32_t cam_height;
  uint32_t cam_width;
  float cam_framerate;
  bool cam_verbose;
  bool is_saved;
  bool is_active;
  bool operator== (const RPiHTConfigData& other) const { return device_path == other.device_path && ip == other.ip && port == other.port && cam_height == other.cam_height && cam_width == other.cam_width && cam_framerate == other.cam_framerate && cam_verbose == other.cam_verbose; }
};


class RPiHTConfig
{
public:
  RPiHTConfig(std::string&);
  ~RPiHTConfig();
  RPiHTConfig(const RPiHTConfig&) = delete; // no copy constructor allowed
  RPiHTConfig& operator=(const RPiHTConfig&) = delete; // no copy assignment allowed
  RPiHTConfigData& get_data() { return data; }

  void set_data(const RPiHTConfigData&);
  [[nodiscard]]std::string get_device_path() const { return data.device_path; }
  void set_device_path(const std::string& dp) { data.device_path = dp; data.is_active = data.is_saved = false; }
  [[nodiscard]] std::string get_ip() const { return data.ip; }
  void set_ip(const std::string& dp) { data.ip = dp; data.is_active = data.is_saved = false; }

  void save();
  void print();
  void set_active(const bool& ia) { data.is_active = ia; }
private:
  RPiHTConfigData data;
  class Impl;
  std::unique_ptr<Impl> pImpl;

};









