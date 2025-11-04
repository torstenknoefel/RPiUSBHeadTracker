#pragma once

#include "RPiHTConfig.h"
#include "RPiHTCam.h"


class RPiHTWebApp
{
public:
  RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam);
  ~RPiHTWebApp();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};









