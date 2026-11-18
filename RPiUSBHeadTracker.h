// RPiUSBHeadTracker.h : Include file for standard system include files,
// or project specific include files.



#pragma once

#include "RPiHTConfig.h"
#include "RPiHTWebApp.h"
#include "RPiHTCam.h"
#include "HeadTracker.h"
#include "USBJoystick.h"

// Face model for the head tracker, downloaded by CMake into the build folder.
// Like the web pages it is loaded relative to the folder the program runs in.
#define MODEL_DIR "models"  // face model files, relative to the data folder (-d, default: current folder)

class RPiUSBHeadTracker
{

public:
  // Members are created in this order and destroyed in reverse:
  // the tracker stops before the camera it reads from.
  RPiHTCam cam;
  HeadTracker tracker;
  RPiHTWebApp webapp;
  USBJoystick joystick;
  RPiUSBHeadTracker(RPiHTConfig &config, Trial &trial);
  ~RPiUSBHeadTracker();
  // "" if camera, web server and joystick work; otherwise what does not
  std::string startup_problem();
  void run();
private:
  Trial& trial;

};


