#pragma once
#include <string>
#include <memory>



struct rpiht_config
{
  std::string device_path;
  std::string ip;
  int port;
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









