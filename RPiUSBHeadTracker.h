// RPiUSBHeadTracker.h : Include file for standard system include files,
// or project specific include files.



#pragma once

#include "RPiHTConfig.h"
#include "RPiHTWebApp.h"
#include "RPiHTCam.h"
#include "USBJoystick.h"

class RPiUSBHeadTracker
{

public:
  uint16_t i = 0;
  int16_t throttle = 0;
  int16_t rudder = 0;
  int16_t x = 0;
  int16_t y = 0;
  int32_t radius = 120;
  int32_t angle = 0;
  uint8_t tmp = 0;
  uint32_t buttons = 0;
  uint8_t hat = 0;


  RPiHTCam cam;
  RPiHTWebApp webapp;
  USBJoystick joystick;
  RPiUSBHeadTracker(RPiHTConfig &config);
  ~RPiUSBHeadTracker();
  void run();
private:

};


