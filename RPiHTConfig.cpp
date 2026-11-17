#include <iostream>
#include <fstream>

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
    TAO_JSON_BIND_OPTIONAL("saved", &RPiHTConfigData::is_saved),
    TAO_JSON_BIND_OPTIONAL("active", &RPiHTConfigData::is_active)
  >
  {
  };
};


class RPiHTConfig::Impl
{
public:
  RPiHTConfigData* config_data;
  Impl(RPiHTConfigData* cfg_ptr, std::string& path)
  {
    config_data = cfg_ptr;
    config_path = path;
    tao::config::internal::try_catch([=]() {
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
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    tao::config::value value;
    value.assign(data);
    tao::json::to_stream(file, value, 3);
    return file.good();
  };
  void print()
  {
    v.assign(*config_data);
    tao::json::to_stream(std::cout, v, 3);
  };
  void save()
  {
    std::ofstream config_file;
    config_file.open(config_path, std::ios::out | std::ios::trunc);
    config_data->is_saved = true; // set before writing, so the file records the saved state
    v.assign(*config_data);
    tao::json::to_stream(std::cout, v, 3);
    std::cout << "...saving config..." << std::endl;
    tao::json::to_stream(config_file, v, 3);
    if (!config_file.good())
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
