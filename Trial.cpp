#include "Trial.h"

#include <cmath>
#include <filesystem>
#include <iostream>

Trial::Trial(RPiHTConfig& config, std::string marker_path, std::chrono::seconds timeout)
  : config_(config), marker_path_(std::move(marker_path)), timeout_(timeout)
{ }

void Trial::start(const RPiHTConfigData& last_good)
{
  std::lock_guard<std::mutex> lock(mtx_);
  active_ = true;
  counting_ = false;
  last_good_ = last_good;
  failure_.clear();
  if (!config_.write_file(marker_path_, last_good_))
    std::cerr << "Trial: could not write " << marker_path_ << std::endl;
}

void Trial::started(clock::time_point now)
{
  std::lock_guard<std::mutex> lock(mtx_);
  if (active_ && !counting_) {
    counting_ = true;
    deadline_ = now + timeout_;
  }
}

void Trial::keep()
{
  std::lock_guard<std::mutex> lock(mtx_);
  active_ = counting_ = false;
  remove_marker();
}

RPiHTConfigData Trial::cancel()
{
  std::lock_guard<std::mutex> lock(mtx_);
  active_ = counting_ = false;
  remove_marker();
  return last_good_;
}

RPiHTConfigData Trial::fail(const std::string& reason)
{
  std::lock_guard<std::mutex> lock(mtx_);
  active_ = counting_ = false;
  failure_ = reason;
  remove_marker();
  std::cerr << "Trial: back to the last working settings: " << reason << std::endl;
  return last_good_;
}

bool Trial::active() const
{
  std::lock_guard<std::mutex> lock(mtx_);
  return active_;
}

bool Trial::expired(clock::time_point now) const
{
  std::lock_guard<std::mutex> lock(mtx_);
  return active_ && counting_ && now >= deadline_;
}

int Trial::seconds_left(clock::time_point now) const
{
  std::lock_guard<std::mutex> lock(mtx_);
  if (!active_)
    return -1;
  if (!counting_)
    return int(timeout_.count());
  const double left = std::chrono::duration<double>(deadline_ - now).count();
  return left > 0 ? int(std::ceil(left)) : 0;
}

std::string Trial::failure() const
{
  std::lock_guard<std::mutex> lock(mtx_);
  return failure_;
}

void Trial::dismiss()
{
  std::lock_guard<std::mutex> lock(mtx_);
  failure_.clear();
}

bool Trial::recover(RPiHTConfigData& last_good)
{
  std::lock_guard<std::mutex> lock(mtx_);
  std::error_code ec;
  if (!std::filesystem::exists(marker_path_, ec))
    return false;
  RPiHTConfigData saved = config_.get_data();
  const bool ok = config_.read_file(marker_path_, saved);
  remove_marker();
  if (!ok)
    return false;
  last_good = saved;
  active_ = counting_ = false;
  failure_ = "The program stopped while trying new settings.";
  return true;
}

void Trial::remove_marker()
{
  std::error_code ec;
  std::filesystem::remove(marker_path_, ec);
}
