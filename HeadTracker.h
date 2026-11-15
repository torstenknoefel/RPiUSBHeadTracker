#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include <opencv2/objdetect.hpp>

#include "HeadPose.h"
#include "JoystickMapping.h"
#include "OneEuroFilter.h"
#include "RPiHTCam.h"

struct TrackerSettings
{
  cv::Size detect_size{ 256, 192 }; // frames are shrunk to this size for YuNet
  float min_score = 0.6f;           // YuNet confidence for a face
  int centre_frames = 15;           // frames averaged for the centre position
  AxisMapping x{};                  // head turn (yaw)   -> X
  AxisMapping y{};                  // head tilt (pitch) -> Y
  double filter_min_cutoff = 1.0;   // OneEuroFilter, see OneEuroFilter.h
  double filter_beta = 0.05;
  double lost_hold = 0.5;           // s the last value is held when the face is lost
  double lost_fade = 1.0;           // s to return to the centre after that
};

struct TrackerState
{
  uint64_t seq = 0;        // increases with every processed frame
  bool face = false;       // face found in the last frame
  bool centred = false;    // centre position is set
  double yaw = 0;          // filtered angles relative to the centre (degrees)
  double pitch = 0;
  int8_t x = 0;            // joystick output
  int8_t y = 0;
  double processing_ms = 0;  // detection + angles for the last frame
  double latency_ms = 0;     // camera frame received -> joystick values ready
  double fps = 0;            // processed frames per second, averaged over about one second

  // Where the face was found (pixels of the full camera frame), for the web page
  cv::Size frame_size;
  cv::Rect2f box;
  FacePoints points{};
};

// Reads every camera frame in its own thread, finds the face with YuNet,
// works out the head angles and turns them into joystick X/Y.
class HeadTracker
{
public:
  HeadTracker(RPiHTCam& cam, const std::string& model_path, TrackerSettings settings = {});
  ~HeadTracker();
  HeadTracker(const HeadTracker&) = delete;
  HeadTracker& operator=(const HeadTracker&) = delete;

  // false if the face model could not be loaded (the tracker then does nothing)
  bool ok() const { return detector_ != nullptr; }

  // The next head position is used as the new centre.
  void recentre();

  TrackerState state() const;

  // Waits for a state newer than 'last_seq'. Returns false on timeout.
  bool wait_next(TrackerState& out, uint64_t& last_seq, std::chrono::milliseconds timeout) const;

private:
  void loop();
  void process(const cv::Mat& frame, std::chrono::steady_clock::time_point captured);

  RPiHTCam& cam_;
  TrackerSettings settings_;
  cv::Ptr<cv::FaceDetectorYN> detector_;
  std::thread worker_;
  std::atomic_bool running_{ false };

  // Only used by the worker thread
  OneEuroFilter yaw_filter_, pitch_filter_;
  double centre_yaw_ = 0, centre_pitch_ = 0;
  double sum_yaw_ = 0, sum_pitch_ = 0;
  int centre_count_ = 0;
  std::chrono::steady_clock::time_point last_face_{};
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
  int8_t last_x_ = 0, last_y_ = 0;
  int fps_count_ = 0;
  double fps_ = 0;
  std::chrono::steady_clock::time_point fps_start_ = std::chrono::steady_clock::now();

  std::atomic_bool recentre_{ true };
  mutable std::mutex mtx_;
  mutable std::condition_variable updated_;
  TrackerState state_; // guarded by mtx_
};
