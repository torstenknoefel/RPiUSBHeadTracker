#pragma once
#include <string>
#include <memory>



struct rpiht_config
{
  std::string device_path;
  std::string ip;
  int port;
  uint32_t cam_height;
  uint32_t cam_width;
  float cam_framerate;
  bool cam_verbose;
};

class RPiHTConfig
{

public:
  rpiht_config data;
  RPiHTConfig(std::string&);
  ~RPiHTConfig();
  RPiHTConfig(const RPiHTConfig&) = delete; // no copy constructor allowed
  RPiHTConfig& operator=(const RPiHTConfig&) = delete; // no copy assignment allowed

  void save();
  void print();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};









