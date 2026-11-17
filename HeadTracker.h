#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <opencv2/objdetect.hpp>

#include "HeadPose.h"
#include "JoystickMapping.h"
#include "NeuralPose.h"
#include "OneEuroFilter.h"
#include "RPiHTCam.h"
#include "TrackerSettings.h"

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
  std::string model;         // face model in use: "YuNet" or "NeuralNet"

  // Where the face was found (pixels of the full camera frame), for the web page
  cv::Size frame_size;
  cv::Rect2f box;
  bool has_points = false;   // only YuNet finds the five points
  FacePoints points{};
};

// Reads every camera frame in its own thread, finds the face with the chosen
// face model (YuNet or NeuralNet), works out the head angles and turns them
// into joystick X/Y.
class HeadTracker
{
public:
  // 'model_dir': folder with the model files (see CMakeLists.txt)
  HeadTracker(RPiHTCam& cam, const std::string& model_dir, TrackerSettings settings = {});
  ~HeadTracker();
  HeadTracker(const HeadTracker&) = delete;
  HeadTracker& operator=(const HeadTracker&) = delete;

  // false if the face model could not be loaded (the tracker then does nothing)
  bool ok() const { return detector_ != nullptr; }

  // The next head position is used as the new centre.
  void recentre();

  // New settings, used from the next camera frame on (no restart, the centre stays).
  void update_settings(const TrackerSettings& settings);

  TrackerState state() const;

  // Waits for a state newer than 'last_seq'. Returns false on timeout.
  bool wait_next(TrackerState& out, uint64_t& last_seq, std::chrono::milliseconds timeout) const;

private:
  void loop();
  void process(const cv::Mat& frame, std::chrono::steady_clock::time_point captured);
  // The face model to use now: the chosen one, or YuNet if NeuralNet cannot be loaded
  FaceModel usable_model();
  HeadPose detect_yunet(const cv::Mat& frame, cv::Rect2f& box, FacePoints& points, bool& has_points);
  HeadPose detect_neural(const cv::Mat& frame, cv::Rect2f& box);

  RPiHTCam& cam_;
  const std::string model_dir_;
  TrackerSettings settings_;
  cv::Ptr<cv::FaceDetectorYN> detector_;
  std::unique_ptr<NeuralPose> neural_;  // loaded when first chosen
  bool neural_failed_ = false;
  FaceModel used_ = FaceModel::YuNet;
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
  std::mutex settings_mtx_;
  TrackerSettings new_settings_;           // guarded by settings_mtx_
  std::atomic_bool settings_changed_{ false };
  mutable std::mutex mtx_;
  mutable std::condition_variable updated_;
  TrackerState state_; // guarded by mtx_
};
