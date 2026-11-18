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
#include <cerrno>
#include <chrono>
#include <cstring>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include "RPiHTConfig.h"
#include "RPiHTWebApp.h"
#include "RPiHTCam.h"
#include "Trial.h"

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

RPiUSBHeadTracker::RPiUSBHeadTracker(RPiHTConfig& config, Trial& trial)
  : cam(config)
  , tracker(cam, MODEL_DIR, tracker_settings(config.get_data()))
  , webapp(config, cam, tracker, trial)
  , joystick(config.get_device_path())
  , trial(trial)
{ }

std::string RPiUSBHeadTracker::startup_problem()
{
  if (!webapp.started())
    return "The web server could not start (port taken or not allowed, or wrong address).";
  if (!joystick.ok())
    return "The joystick device could not be opened.";
  cv::Mat frame;
  uint64_t seq = 0;
  if (!cam.wait_next(frame, seq, std::chrono::seconds(5)))
    return "The camera delivered no picture.";
  return "";
}

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

    if (trial.expired())
      webapp.trial_expired();  // not confirmed in time: back to the last working settings

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
        printf(" | Tracker (%s): %.1f frames/s, face %d%%, processing %.1f ms, latency %.1f ms (max %.1f) | "
               "yaw %.1f pitch %.1f -> x %d y %d%s",
               s.model.c_str(), states / elapsed.count(), 100 * faces / states, sum_processing / states,
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
  parser.set_optional<std::string>("d", "datadir", "",
    "Folder with the web pages (templates, static) and face models (models); default: the current folder");
  parser.run_and_exit_if_error();
  // Absolute, so it stays right after changing to the data folder
  auto configpath = std::filesystem::absolute(parser.get<std::string>("c")).string();
  const auto datadir = parser.get<std::string>("d");
  if (!datadir.empty() && chdir(datadir.c_str()) != 0) {
    fprintf(stderr, "Cannot use the data folder %s: %s\n", datadir.c_str(), strerror(errno));
    return 1;
  }

  RPiHTConfig config(configpath);
  printf("Current Configuration:\n\r");
  config.print();

  // Settings being tried are kept only when confirmed; the last working ones
  // wait in this file meanwhile. If it is still there, the program stopped
  // during a trial: start with the last working settings.
  Trial trial(config, configpath + ".trial");
  RPiHTConfigData last_good;
  if (trial.recover(last_good)) {
    printf("The program stopped while trying new settings - using the last working settings\n\r");
    config.set_data(last_good);
  }

  install_signal_handlers();

  do
  {
    try {
      RPiUSBHeadTracker app(config, trial);
      const std::string problem = app.startup_problem();
      if (!problem.empty()) {
        printf("%s\n\r", problem.c_str());
        if (trial.active()) {
          config.set_data(trial.fail(problem));  // back to the last working settings
          continue;
        }
      }
      trial.started();  // the countdown for "Keep" starts now
      config.set_active(true);
      app.run();
    }
    catch (const std::exception& e) {
      printf("Start failed: %s\n\r", e.what());
      if (trial.active())
        config.set_data(trial.fail(std::string("Start failed: ") + e.what()));
      else
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  } while (run);

  printf("Headtracker stopped\n\r");
}

