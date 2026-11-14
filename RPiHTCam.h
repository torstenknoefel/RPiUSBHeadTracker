#pragma once
#include <lccv.hpp>
#include <opencv2/opencv.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "RPiHTConfig.h"

// Owns the camera. One thread reads every frame from the camera and keeps the
// newest one; any number of readers (head tracker, web page) get copies of it.
// LCCV hands each frame to only one caller of getVideoFrame(), so nothing else
// may read from the camera directly.
class RPiHTCam
{
public:
  explicit RPiHTCam(RPiHTConfig&);
  ~RPiHTCam();
  RPiHTCam(const RPiHTCam&) = delete;
  RPiHTCam& operator=(const RPiHTCam&) = delete;

  // Copies the newest frame into 'frame'. Returns false if there is none yet.
  // 'seq' (optional) receives the frame's sequence number.
  bool latest(cv::Mat& frame, uint64_t* seq = nullptr) const;

  // Waits until a frame newer than 'last_seq' arrives, copies it and updates
  // 'last_seq'. 'captured' (optional) receives the time the frame arrived from
  // the camera. Returns false on timeout or when the camera stops.
  bool wait_next(cv::Mat& frame, uint64_t& last_seq, std::chrono::milliseconds timeout,
                 std::chrono::steady_clock::time_point* captured = nullptr) const;

  // Frames per second delivered by the camera, averaged over about one second.
  double fps() const { return fps_.load(); }

  uint32_t num_cams = 0;
  uint32_t height = 0;
  uint32_t width = 0;

private:
  void capture_loop();

  lccv::PiCamera cam;
  std::thread worker;
  std::atomic_bool running{ false };
  std::atomic<double> fps_{ 0.0 };

  mutable std::mutex mtx;
  mutable std::condition_variable new_frame;
  cv::Mat newest;    // guarded by mtx
  uint64_t seq_ = 0; // guarded by mtx; 0 = no frame yet
  std::chrono::steady_clock::time_point newest_time; // guarded by mtx
};
