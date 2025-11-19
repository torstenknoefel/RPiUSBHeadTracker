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
  struct traits< rpiht_config > : binding::object<
    TAO_JSON_BIND_REQUIRED("device", &rpiht_config::device_path),
    TAO_JSON_BIND_OPTIONAL("ip", &rpiht_config::ip),
    TAO_JSON_BIND_REQUIRED("cam_width", &rpiht_config::cam_width),
    TAO_JSON_BIND_REQUIRED("cam_height", &rpiht_config::cam_height),
    TAO_JSON_BIND_REQUIRED("cam_framerate", &rpiht_config::cam_framerate),
    TAO_JSON_BIND_REQUIRED("cam_verbose", &rpiht_config::cam_verbose),
    TAO_JSON_BIND_REQUIRED("port", &rpiht_config::port)
  >
  {
  };
};


class RPiHTConfig::Impl
{
public:
  rpiht_config* config_data;
  Impl(rpiht_config* cfg_ptr, std::string& path)
  {
    config_data = cfg_ptr;
    config_path = path;
    tao::config::internal::try_catch([=]() {
      v = tao::config::from_file(std::filesystem::path(path));
      *config_data = v.as <rpiht_config>(); //TODO: Error handling test?
      });
  };
  ~Impl() = default;
  void print()
  {
    v.assign(*config_data);
    tao::json::to_stream(std::cout, v, 3);
  };
  void save()
  {
    std::ofstream config_file;
    config_file.open(config_path, std::ios::out | std::ios::trunc);
    v.assign(*config_data);
    tao::json::to_stream(std::cout, v, 3);
    std::cout << "...saving config..." << std::endl;
    tao::json::to_stream(config_file, v, 3);
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