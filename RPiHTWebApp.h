#pragma once

#include "RPiHTConfig.h"
#include "RPiHTCam.h"
#include "HeadTracker.h"


class RPiHTWebApp
{
public:
  RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam, HeadTracker &tracker);
  ~RPiHTWebApp();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};









