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

#include <algorithm>
#include <chrono>
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

// Tracker settings from the config file, with lower limits so that odd values
// from the web form cannot stop the tracker.
static TrackerSettings tracker_settings(const RPiHTConfigData& d)
{
  TrackerSettings s;
  s.x.range = std::max(d.track_range_x, 1.0);
  s.y.range = std::max(d.track_range_y, 1.0);
  s.x.deadzone = s.y.deadzone = std::max(d.track_deadzone, 0.0);
  s.x.curve = s.y.curve = std::max(d.track_curve, 0.1);
  s.x.invert = d.track_invert_x;
  s.y.invert = d.track_invert_y;
  s.filter_min_cutoff = std::max(d.track_smoothing, 0.05);
  s.filter_beta = std::max(d.track_speed, 0.0);
  s.detect_size = detect_size_for(d.track_detect_width, cv::Size(int(d.cam_width), int(d.cam_height)));
  return s;
}

RPiUSBHeadTracker::RPiUSBHeadTracker(RPiHTConfig& config)
  : cam(config)
  , tracker(cam, FACE_MODEL_PATH, tracker_settings(config.get_data()))
  , webapp(config, cam, tracker)
  , joystick(config.get_device_path())
{ }

RPiUSBHeadTracker::~RPiUSBHeadTracker() = default;

void RPiUSBHeadTracker::run()
{
  printf("Started Headtracker\n\r");
  reload_config = false;

  if (!tracker.ok())
    printf("Head tracker not available (face model missing?) - joystick stays centred\n\r");

  // Statistics for the log line every 10 s
  using steady = std::chrono::steady_clock;
  auto window_start = steady::now();
  int states = 0, faces = 0;
  double sum_processing = 0, sum_latency = 0, max_latency = 0;

  uint64_t seq = 0;
  TrackerState s;

  while (::run && not reload_config) { // ::run is the global flag; plain "run" would be this member function

    if (tracker.ok()) {
      // One joystick report per processed camera frame
      if (tracker.wait_next(s, seq, std::chrono::milliseconds(200))) {
        joystick.update(0, 0, s.x, s.y, 0, JOY_HAT_NEUTRAL);
        ++states;
        faces += s.face;
        sum_processing += s.processing_ms;
        sum_latency += s.latency_ms;
        max_latency = std::max(max_latency, s.latency_ms);
      }
    }
    else {
      joystick.update(0, 0, 0, 0, 0, JOY_HAT_NEUTRAL);
      usleep(100000);
    }

    const std::chrono::duration<double> elapsed = steady::now() - window_start;
    if (elapsed.count() >= 10.0) {
      printf("Camera: %.1f frames/s", cam.fps());
      if (states > 0)
        printf(" | Tracker: %.1f frames/s, face %d%%, processing %.1f ms, latency %.1f ms (max %.1f) | "
               "yaw %.1f pitch %.1f -> x %d y %d%s",
               states / elapsed.count(), 100 * faces / states, sum_processing / states,
               sum_latency / states, max_latency, s.yaw, s.pitch, s.x, s.y, s.centred ? "" : " (centring)");
      printf("\n\r");
      fflush(stdout);
      window_start = steady::now();
      states = faces = 0;
      sum_processing = sum_latency = max_latency = 0;
    }
  }
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

