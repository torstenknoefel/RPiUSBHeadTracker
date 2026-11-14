#include <iostream>

#include <libcamera_app.hpp>

#include "RPiHTCam.h"

RPiHTCam::RPiHTCam(RPiHTConfig& config)
{
  num_cams = LibcameraApp::GetNumberCameras();
  std::cout << "Found " << num_cams << " cameras." << std::endl;

  const auto& data = config.get_data();
  height = data.cam_height;
  width = data.cam_width;

  cam.options->video_width = width;
  cam.options->video_height = height;
  cam.options->framerate = data.cam_framerate;
  cam.options->verbose = data.cam_verbose;
  cam.startVideo();

  running = true;
  worker = std::thread(&RPiHTCam::capture_loop, this);
}

RPiHTCam::~RPiHTCam()
{
  running = false;
  new_frame.notify_all();
  if (worker.joinable())
    worker.join();
  cam.stopVideo();
}

void RPiHTCam::capture_loop()
{
  using clock = std::chrono::steady_clock;
  cv::Mat frame;
  int count = 0;
  auto window_start = clock::now();

  while (running) {
    // Blocks until the camera delivers the next frame (or 1 s timeout)
    if (!cam.getVideoFrame(frame, 1000))
      continue;

    {
      std::lock_guard<std::mutex> lock(mtx);
      std::swap(newest, frame); // no copy; 'frame' is reallocated by the next getVideoFrame if needed
      ++seq_;
    }
    new_frame.notify_all();

    ++count;
    auto now = clock::now();
    std::chrono::duration<double> elapsed = now - window_start;
    if (elapsed.count() >= 1.0) {
      fps_ = count / elapsed.count();
      count = 0;
      window_start = now;
    }
  }
}

bool RPiHTCam::latest(cv::Mat& frame, uint64_t* seq) const
{
  std::lock_guard<std::mutex> lock(mtx);
  if (seq_ == 0)
    return false;
  newest.copyTo(frame);
  if (seq)
    *seq = seq_;
  return true;
}

bool RPiHTCam::wait_next(cv::Mat& frame, uint64_t& last_seq, std::chrono::milliseconds timeout) const
{
  std::unique_lock<std::mutex> lock(mtx);
  if (!new_frame.wait_for(lock, timeout, [&] { return seq_ > last_seq || !running; }))
    return false;
  if (seq_ <= last_seq)
    return false; // stopping
  newest.copyTo(frame);
  last_seq = seq_;
  return true;
}
