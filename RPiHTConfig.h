#pragma once
#include <filesystem>
#include <string>
#include <memory>

// Writes 'text' to 'path' so that a power cut leaves the old or the new file,
// never an empty one (temporary file, flushed, renamed). Where no temporary file
// can be made in that folder, the file is overwritten in place. false on failure.
bool write_file_safely(const std::filesystem::path& path, const std::string& text);



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
  std::string track_model = "yunet";  // face model: "yunet" or "neuralnet" (see NeuralPose.h)

  bool is_saved;   // the values are the ones in the config file
  bool is_active;  // the program runs with these values
  bool operator== (const RPiHTConfigData& other) const
  {
    return same_system(other) && same_tracking(other);
  }
  // Joystick device, web server and camera: changing them needs a restart
  bool same_system(const RPiHTConfigData& other) const
  {
    return device_path == other.device_path && ip == other.ip && port == other.port
      && cam_height == other.cam_height && cam_width == other.cam_width
      && cam_framerate == other.cam_framerate && cam_verbose == other.cam_verbose;
  }
  // Head tracking: these change while the program runs
  bool same_tracking(const RPiHTConfigData& other) const
  {
    return track_range_x == other.track_range_x && track_range_y == other.track_range_y
      && track_deadzone == other.track_deadzone && track_curve == other.track_curve
      && track_smoothing == other.track_smoothing && track_speed == other.track_speed
      && track_invert_x == other.track_invert_x && track_invert_y == other.track_invert_y
      && track_detect_width == other.track_detect_width && track_model == other.track_model;
  }
  // Copies the head tracking values of 'other'
  void copy_tracking(const RPiHTConfigData& other)
  {
    track_range_x = other.track_range_x;
    track_range_y = other.track_range_y;
    track_deadzone = other.track_deadzone;
    track_curve = other.track_curve;
    track_smoothing = other.track_smoothing;
    track_speed = other.track_speed;
    track_invert_x = other.track_invert_x;
    track_invert_y = other.track_invert_y;
    track_detect_width = other.track_detect_width;
    track_model = other.track_model;
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
  // Takes only the head tracking values of 'tracking'. They are used right away,
  // so the config stays active; it is unsaved if a value changed.
  void set_tracking(const RPiHTConfigData& tracking);
  // Reads the config file again (back to the saved values). false if the file
  // cannot be read; the values then stay as they were.
  bool reload();
  // The values in the config file, without changing the current ones.
  // false if the file cannot be read.
  bool read_saved(RPiHTConfigData& out) const;
  // Writes / reads config values to / from another file (e.g. the last working
  // settings while new ones are tried). false on failure.
  bool write_file(const std::string& path, const RPiHTConfigData& d) const;
  bool read_file(const std::string& path, RPiHTConfigData& out) const;
  // Path of the config file the program was started with
  [[nodiscard]] std::string file_path() const;
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









