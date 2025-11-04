


#include "stdint.h"
#include <array>
#include "USBJoystick.h"
#include <iostream>
using namespace std;
bool USBJoystick::update(int16_t t, int16_t r, int16_t x, int16_t y, uint32_t buttons, uint8_t hat) {

  _t = t;
  _r = r;
  _x = x;
  _y = y;
  _buttons = buttons;
  _hat = hat;

  return update();
}

bool USBJoystick::update() {

  std::array <char, 6> report;

  // Fill the report according to the Joystick Descriptor

  report[0] = _x & 0xff;
  report[1] = _y & 0xff;


#if (BUTTONS4 == 1)
  //Hat and 4 Buttons
  //   report.data[4] = ((_buttons & 0x0f) << 4) | (_hat & 0x0f) ;
  //   report.length = 5;


  //Use 4 bit padding for hat4 or hat8
  report.data[4] = (_hat & 0x0f);

  //Use 4 bit padding for buttons
  report.data[5] = (_buttons & 0x0f);
  report.length = 6;
#endif

#if (BUTTONS8 == 1)
  //Hat and first 4 Buttons
  //   report.data[4] = ((_buttons & 0x0f) << 4) | (_hat & 0x0f) ;
  //
  //Use bit padding for last 4 Buttons
  //   report.data[5] =  (_buttons & 0xf0) >> 4;
  //   report.length = 6;

  //Use 4 bit padding for hat4 or hat8
  report.data[4] = (_hat & 0x0f);

  //Use 8 bits for buttons
  report.data[5] = (_buttons & 0xff);
  report.length = 6;
#endif

#if (BUTTONS32 == 1)
  //Use 4 bit padding for hat4 or hat8
  //report[4] = (_hat & 0x0f);

  //No bit padding for 32 buttons
  report[2] = (_buttons >> 0) & 0xff;
  report[3] = (_buttons >> 8) & 0xff;
  report[4] = (_buttons >> 16) & 0xff;
  report[5] = (_buttons >> 24) & 0xff;
#endif

  fout.write(report.data(), report.size());
  //for (int i = 0; i < 6; i++) {
 //  fout << report[i];
  //}
  fout.flush();

  return USBJoystick::fout.good();
}

bool USBJoystick::throttle(int16_t t) {
  _t = t;
  return update();
}

bool USBJoystick::rudder(int16_t r) {
  _r = r;
  return update();
}

bool USBJoystick::move(int16_t x, int16_t y) {
  _x = x;
  _y = y;
  return update();
}

bool USBJoystick::buttons(uint32_t buttons) {
  _buttons = buttons;
  return update();
}

bool USBJoystick::hat(uint8_t hat) {
  _hat = hat;
  return update();
}


void USBJoystick::_init() {
  _t = -127;
  _r = -127;
  _x = 0;
  _y = 0;
  _buttons = 0x00000000;
  _hat = 0x00;
}


