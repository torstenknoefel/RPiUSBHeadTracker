#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "RPiHTConfig.h"

// Trying new camera / connection settings safely.
//
// start() remembers the last working settings (in memory and in a marker file
// next to the config file) before the program restarts with the new ones.
// The new settings are kept only when the user confirms them in time (keep()).
// Otherwise - not confirmed, the program does not start properly, or it stopped
// while trying (marker file still there at the next start) - the last working
// settings come back.
//
// Thread-safe: used by the main loop and the web server.
class Trial
{
public:
  using clock = std::chrono::steady_clock;

  Trial(RPiHTConfig& config, std::string marker_path,
        std::chrono::seconds timeout = std::chrono::seconds(30));

  // A trial begins; 'last_good' comes back if it fails. Clears an old failure.
  void start(const RPiHTConfigData& last_good);
  // The program runs with the new settings: the countdown for keep() starts.
  void started(clock::time_point now = clock::now());
  // The user keeps the new settings.
  void keep();
  // The user goes back; returns the last working settings.
  RPiHTConfigData cancel();
  // The new settings did not work; returns the last working settings and
  // remembers 'reason' for the web page.
  RPiHTConfigData fail(const std::string& reason);

  bool active() const;
  // true when the countdown has run out without keep()
  bool expired(clock::time_point now = clock::now()) const;
  // Seconds until the last working settings come back; -1 if no trial.
  int seconds_left(clock::time_point now = clock::now()) const;
  std::chrono::seconds timeout() const { return timeout_; }

  // Why the last trial was undone ("" if it was not)
  std::string failure() const;
  void dismiss();

  // At program start: if the marker file is still there, the program stopped
  // while trying new settings. Puts the last working settings into 'last_good',
  // remembers the failure and returns true.
  bool recover(RPiHTConfigData& last_good);

private:
  void remove_marker();

  RPiHTConfig& config_;
  const std::string marker_path_;
  const std::chrono::seconds timeout_;
  mutable std::mutex mtx_;
  bool active_ = false;
  bool counting_ = false;
  clock::time_point deadline_{};
  RPiHTConfigData last_good_{};
  std::string failure_;
};
