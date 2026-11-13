// RPiUSBHeadTracker.cpp : Defines the entry point for the application.
//

#include "RPiUSBHeadTracker.h"

using namespace std;

#include "USBJoystick.h"

#include "CmdParser.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include <csignal>
#include <filesystem>
#include <iostream>
#include <vector>

#include "RPiHTConfig.h"
#include "RPiHTWebApp.h"
#include "RPiHTCam.h"

std::atomic_bool run = true;    // we need this in either config or a separate status class,probably the latter:
std::atomic_bool reload_config = false;                        // a Class in a header file and getter and setters
static_assert(std::atomic_bool::is_always_lock_free, "run/reload_config are set from a signal handler");

// SIGINT/SIGTERM: leave the main loop and shut down cleanly (camera, web server, joystick device)
extern "C" void handle_stop_signal(int)
{
  run = false;
  reload_config = true;
}

static void install_signal_handlers()
{
  struct sigaction sa {};
  sa.sa_handler = handle_stop_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

RPiUSBHeadTracker::RPiUSBHeadTracker(RPiHTConfig& config)
  : cam(config)
  , webapp(config, cam)
  , joystick(config.get_device_path())
{ }

RPiUSBHeadTracker::~RPiUSBHeadTracker() = default;

void RPiUSBHeadTracker::run()
{
  printf("Started Headtracker\n\r");
  reload_config = false;

  while (::run && not reload_config) { // ::run is the global flag; plain "run" would be this member function

    // Basic Joystick
    throttle = (i >> 8) & 0xFF; // value -127 .. 128
    rudder = (i >> 8) & 0xFF;   // value -127 .. 128

#if (BUTTONS4 == 1)
    buttons = (i >> 8) & 0x0F;   // value    0 ..  15, one bit per button
#endif
#if (BUTTONS8 == 1)
    buttons = (i >> 8) & 0xFF;   // value    0 .. 255, one bit per button
#endif
#if (BUTTONS32 == 1)
    tmp = (i >> 8) & 0xFF;   // value    0 .. 255, one bit per button
    buttons = ((tmp << 0) & 0x000000FF);
    buttons = buttons | ((~tmp << 8) & 0x0000FF00);
    buttons = buttons | ((tmp << 16) & 0x00FF0000);
    buttons = buttons | ((~tmp << 24) & 0xFF000000);
#endif

#if (HAT4 == 1)
    hat = (i >> 8) & 0x03;   // value 0, 1, 2, 3 or 4 for neutral
#endif
#if (HAT8 == 1)
    hat = (i >> 8) & 0x07;   // value 0..7 or 8 for neutral
#endif
    i++;

    x = cos((double)angle * 3.14 / 180.0) * radius;  // value -127 .. 128
    y = sin((double)angle * 3.14 / 180.0) * radius;  // value -127 .. 128
    angle += 3;

    joystick.update(throttle, rudder, x, y, buttons, hat);
    usleep(100000); // 100 ms; sleep() takes whole seconds, so sleep(0.1) was sleep(0)
  }


  // Advanced Joystick
 // hat = (1 >> 8) & 0x07;   // value 0..7 or 8 for neutral
 // tmp = (1 >> 8) & 0xFF;   // value    0 .. 255, one bit per button
 // buttons = ((tmp << 0) & 0x000000FF);
 // buttons = buttons | ((~tmp << 8) & 0x0000FF00);
 // buttons = buttons | ((tmp << 16) & 0x00FF0000);
 // buttons = buttons | ((~tmp << 24) & 0xFF000000);
 // for (angle = 0; angle < 360; angle += 5) {
 //   x = cos((double)angle * 3.14 / 180.0) * radius;  // value -127 .. 128
 //   y = sin((double)angle * 3.14 / 180.0) * radius;  // value -127 .. 128
 //   throttle = (sin((double)angle * 3.14 / 180.0) + 1) * 127; // value   0 .. 255
 //   rudder = (cos((double)angle * 3.14 / 180.0) + 1) * 127;   // value   0 .. 255()
 //   joystick.update(throttle, rudder, x, y, buttons, hat);
 //   usleep(100000);
//  }
}

int main(int argc, char** argv) {
  cli::Parser parser(argc, argv);
  parser.set_optional<std::string>("c", "configfile", "/etc/USBHT.json", "Full path to JSON config file");
  //parser.set_optional<int>("n", "number", 8, "Integers in all forms, e.g., unsigned int, long long, ..., are possible. Hexadecimal and Ocatl numbers parsed as well");
  //parser.set_optional<cli::NumericalBase<int, 10>>("t", "temp", 0, "integer parsing restricted only to numerical base 10");
  //parser.set_optional<double>("b", "beta", 11.0, "Also floating point values are possible.");
  //parser.set_optional<bool>("a", "all", false, "Boolean arguments are simply switched when encountered, i.e. false to true if provided.");
  //parser.set_required<std::vector<short>>("v", "values", "By using a vector it is possible to receive a multitude of inputs.");
  parser.run_and_exit_if_error();
  auto configpath = parser.get<std::string>("c");

  RPiHTConfig config(configpath);
  printf("Current Configuration:\n\r");
  config.print();

  install_signal_handlers();

  do
  {
    RPiUSBHeadTracker app(config);
    config.set_active(true);
    app.run();
  } while (run);

  printf("Headtracker stopped\n\r");
}

