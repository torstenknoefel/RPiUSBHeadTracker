#include <cerrno>
#include <iostream>
#include <fstream>
#include <sstream>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <tao/config.hpp>

#include <tao/json.hpp>
#include <tao/json/contrib/traits.hpp>
#include <tao/json/jaxn/events/to_stream.hpp>

#include "try_catch.hpp"

#include "RPiHTConfig.h"


#define CONFIGFILE "/etc/USBHT.json"  //TODO: make configurable via cmd line arg

namespace tao::json
{
  template<>
  struct traits< RPiHTConfigData > : binding::object<
    TAO_JSON_BIND_REQUIRED("device", &RPiHTConfigData::device_path),
    TAO_JSON_BIND_OPTIONAL("ip", &RPiHTConfigData::ip),
    TAO_JSON_BIND_REQUIRED("cam_width", &RPiHTConfigData::cam_width),
    TAO_JSON_BIND_REQUIRED("cam_height", &RPiHTConfigData::cam_height),
    TAO_JSON_BIND_REQUIRED("cam_framerate", &RPiHTConfigData::cam_framerate),
    TAO_JSON_BIND_REQUIRED("cam_verbose", &RPiHTConfigData::cam_verbose),
    TAO_JSON_BIND_REQUIRED("port", &RPiHTConfigData::port),
    TAO_JSON_BIND_OPTIONAL("track_range_x", &RPiHTConfigData::track_range_x),
    TAO_JSON_BIND_OPTIONAL("track_range_y", &RPiHTConfigData::track_range_y),
    TAO_JSON_BIND_OPTIONAL("track_deadzone", &RPiHTConfigData::track_deadzone),
    TAO_JSON_BIND_OPTIONAL("track_curve", &RPiHTConfigData::track_curve),
    TAO_JSON_BIND_OPTIONAL("track_smoothing", &RPiHTConfigData::track_smoothing),
    TAO_JSON_BIND_OPTIONAL("track_speed", &RPiHTConfigData::track_speed),
    TAO_JSON_BIND_OPTIONAL("track_invert_x", &RPiHTConfigData::track_invert_x),
    TAO_JSON_BIND_OPTIONAL("track_invert_y", &RPiHTConfigData::track_invert_y),
    TAO_JSON_BIND_OPTIONAL("track_detect_width", &RPiHTConfigData::track_detect_width),
    TAO_JSON_BIND_OPTIONAL("track_model", &RPiHTConfigData::track_model),
    TAO_JSON_BIND_OPTIONAL("track_threads", &RPiHTConfigData::track_threads),
    TAO_JSON_BIND_OPTIONAL("saved", &RPiHTConfigData::is_saved),
    TAO_JSON_BIND_OPTIONAL("active", &RPiHTConfigData::is_active)
  >
  {
  };
};


namespace
{
  // Writes all of 'text' to the open file and flushes it to the card
  bool write_all(int fd, const std::string& text)
  {
    size_t done = 0;
    while (done < text.size()) {
      const ssize_t n = ::write(fd, text.data() + done, text.size() - done);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        return false;
      done += size_t(n);
    }
    return ::fsync(fd) == 0;
  }
}

// Writes 'text' to 'path' so that a power cut (the Pi is often unplugged
// together with the PC) leaves either the old or the new file, never an empty
// one: into a temporary file in the same folder, flushed to the card, then
// renamed over the old file. If no file can be made in that folder (for example
// /etc for a normal user), the file is overwritten in place instead (flushed too).
bool write_file_safely(const std::filesystem::path& path, const std::string& text)
{
  struct stat old {};
  const bool exists = ::stat(path.c_str(), &old) == 0;
  const mode_t mode = exists ? (old.st_mode & 07777) : 0644;
  // A file the user may not write stays as it is (renaming over it would work)
  if (exists && ::access(path.c_str(), W_OK) != 0)
    return false;

  std::string temp = path.string() + ".XXXXXX";
  const int fd = ::mkstemp(temp.data());
  if (fd >= 0) {
    ::fchmod(fd, mode);  // keep the old file's permissions
    const bool written = write_all(fd, text);
    const bool closed = ::close(fd) == 0;
    if (written && closed && ::rename(temp.c_str(), path.c_str()) == 0) {
      // The new name must reach the card too
      const std::string dir = path.has_parent_path() ? path.parent_path().string() : ".";
      const int dfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
      if (dfd >= 0) {
        ::fsync(dfd);
        ::close(dfd);
      }
      return true;
    }
    ::unlink(temp.c_str());
    if (!written || !closed)
      return false;  // the folder takes files, but writing failed (card full?)
  }
  // No temporary file possible in that folder: overwrite in place
  const int out = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
  if (out < 0)
    return false;
  const bool ok = write_all(out, text);
  return ::close(out) == 0 && ok;
}

class RPiHTConfig::Impl
{
public:
  RPiHTConfigData* config_data;
  Impl(RPiHTConfigData* cfg_ptr, std::string& path)
  {
    config_data = cfg_ptr;
    config_path = path;
    tao::config::internal::try_catch([&]() {
      v = tao::config::from_file(std::filesystem::path(path));
      *config_data = v.as <RPiHTConfigData>(); //TODO: Error handling test?
      });
    config_data->is_saved = true;
    config_data->is_active = true;
  };
  ~Impl() = default;
  const std::filesystem::path& path() const { return config_path; }
  bool read(RPiHTConfigData& out) const
  {
    return read_from(config_path, out);
  };
  static bool read_from(const std::filesystem::path& path, RPiHTConfigData& out)
  {
    bool ok = false;
    tao::config::internal::try_catch([&]() {
      out = tao::config::from_file(path).as <RPiHTConfigData>();
      ok = true;
      });
    return ok;
  };
  static bool write_to(const std::filesystem::path& path, const RPiHTConfigData& data)
  {
    tao::config::value value;
    value.assign(data);
    std::ostringstream text;
    tao::json::to_stream(text, value, 3);
    return write_file_safely(path, text.str());
  };
  void print()
  {
    v.assign(*config_data);
    tao::json::to_stream(std::cout, v, 3);
  };
  void save()
  {
    config_data->is_saved = true; // set before writing, so the file records the saved state
    v.assign(*config_data);
    tao::json::to_stream(std::cout, v, 3);
    std::cout << "...saving config..." << std::endl;
    std::ostringstream text;
    tao::json::to_stream(text, v, 3);
    if (!write_file_safely(config_path, text.str()))
    {
      config_data->is_saved = false;
      std::cerr << "Error: could not write config file " << config_path << std::endl;
    }
  };
private:
  tao::config::value v;
  std::filesystem::path config_path;
};

RPiHTConfig::RPiHTConfig(std::string& path) : pImpl(std::make_unique<Impl>(&data, path))
{ };

RPiHTConfig::~RPiHTConfig() = default;  // TODO: write own to destructor to ensure data exists until pImpl is destroyed.

void RPiHTConfig::save()
{
  pImpl->save();
};

void RPiHTConfig::print()
{
  pImpl->print();
};
void RPiHTConfig::set_data(const RPiHTConfigData& new_data)
{
  if (!(data == new_data))
  {
    data = new_data;
    data.is_saved = false;
    data.is_active = false;
  }
}

void RPiHTConfig::set_tracking(const RPiHTConfigData& tracking)
{
  if (!data.same_tracking(tracking))
  {
    data.copy_tracking(tracking);
    data.is_saved = false;
  }
}

bool RPiHTConfig::read_saved(RPiHTConfigData& out) const
{
  out = data;  // stays like this if the file cannot be read
  return pImpl->read(out);
}

bool RPiHTConfig::reload()
{
  RPiHTConfigData saved;
  if (!read_saved(saved))
    return false;
  const bool changed = !(saved == data);
  data = saved;
  data.is_saved = true;
  if (changed)
    data.is_active = false;
  return true;
}

bool RPiHTConfig::write_file(const std::string& path, const RPiHTConfigData& d) const
{
  return Impl::write_to(path, d);
}

bool RPiHTConfig::read_file(const std::string& path, RPiHTConfigData& out) const
{
  return Impl::read_from(path, out);
}

std::string RPiHTConfig::file_path() const
{
  return pImpl->path().string();
}
