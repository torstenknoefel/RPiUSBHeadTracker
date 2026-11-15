#pragma once

#include "RPiHTConfig.h"
#include "RPiHTCam.h"
#include "HeadTracker.h"
#include "Trial.h"


class RPiHTWebApp
{
public:
  RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam, HeadTracker &tracker, Trial &trial);
  ~RPiHTWebApp();

  // false if the web server could not start (port taken or not allowed, wrong address)
  bool started() const;
  // The trial was not confirmed in time: back to the last working settings
  void trial_expired();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};









