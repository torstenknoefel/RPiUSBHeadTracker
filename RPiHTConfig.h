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

  // Head tracking (all optional in the file; these are the defaults)
  double track_range_x = 20.0;   // degrees of head turn for full X deflection
  double track_range_y = 10.0;   // degrees of head tilt for full Y deflection (nods measure smaller than turns)
  double track_deadzone = 1.5;   // degrees around the centre that give 0
  double track_curve = 1.0;      // 1 = linear, > 1 = finer near the centre
  double track_smoothing = 1.0;  // filter cut-off (Hz) while still: lower = smoother, more lag
  double track_speed = 0.05;     // how quickly the filter follows fast movements: higher = less lag
  bool track_invert_x = false;
  bool track_invert_y = false;
  int track_detect_width = 256;  // camera frames are shrunk to this width for face detection
                                 // (height keeps the camera's aspect): larger = steadier, slower

  bool is_saved;
  bool is_active;
  bool operator== (const RPiHTConfigData& other) const
  {
    return device_path == other.device_path && ip == other.ip && port == other.port
      && cam_height == other.cam_height && cam_width == other.cam_width
      && cam_framerate == other.cam_framerate && cam_verbose == other.cam_verbose
      && track_range_x == other.track_range_x && track_range_y == other.track_range_y
      && track_deadzone == other.track_deadzone && track_curve == other.track_curve
      && track_smoothing == other.track_smoothing && track_speed == other.track_speed
      && track_invert_x == other.track_invert_x && track_invert_y == other.track_invert_y
      && track_detect_width == other.track_detect_width;
  }
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









