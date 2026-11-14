#include "HeadTracker.h"

#include <iostream>

#include <opencv2/imgproc.hpp>

using clock_type = std::chrono::steady_clock;

static double ms_since(clock_type::time_point t)
{
  return std::chrono::duration<double, std::milli>(clock_type::now() - t).count();
}

HeadTracker::HeadTracker(RPiHTCam& cam, const std::string& model_path, TrackerSettings settings)
  : cam_(cam), settings_(settings)
{
  try {
    detector_ = cv::FaceDetectorYN::create(model_path, "", settings_.detect_size, settings_.min_score, 0.3f, 5);
  }
  catch (const cv::Exception& e) {
    std::cerr << "Head tracker: cannot load face model " << model_path << ": " << e.msg << std::endl;
    detector_ = nullptr;
    return;
  }
  yaw_filter_.set_parameters(settings_.filter_min_cutoff, settings_.filter_beta);
  pitch_filter_.set_parameters(settings_.filter_min_cutoff, settings_.filter_beta);
  running_ = true;
  worker_ = std::thread(&HeadTracker::loop, this);
}

HeadTracker::~HeadTracker()
{
  running_ = false;
  if (worker_.joinable())
    worker_.join();
}

void HeadTracker::recentre()
{
  recentre_ = true;
}

TrackerState HeadTracker::state() const
{
  std::lock_guard<std::mutex> lock(mtx_);
  return state_;
}

bool HeadTracker::wait_next(TrackerState& out, uint64_t& last_seq, std::chrono::milliseconds timeout) const
{
  std::unique_lock<std::mutex> lock(mtx_);
  if (!updated_.wait_for(lock, timeout, [&] { return state_.seq > last_seq; }))
    return false;
  out = state_;
  last_seq = state_.seq;
  return true;
}

void HeadTracker::loop()
{
  cv::Mat frame;
  uint64_t frame_seq = 0;
  clock_type::time_point captured;
  while (running_) {
    if (cam_.wait_next(frame, frame_seq, std::chrono::milliseconds(200), &captured))
      process(frame, captured);
  }
}

void HeadTracker::process(const cv::Mat& frame, clock_type::time_point captured)
{
  const auto started = clock_type::now();
  const double t = std::chrono::duration<double>(captured - start_).count();

  // 1. Find the face on a small copy of the frame
  cv::Mat small, faces;
  cv::resize(frame, small, settings_.detect_size, 0, 0, cv::INTER_AREA);
  detector_->detect(small, faces);

  int best = -1;
  for (int r = 0; r < faces.rows; ++r)
    if (best < 0 || faces.at<float>(r, 14) > faces.at<float>(best, 14))
      best = r;

  // 2. Head angles from the five points, in full-frame pixels
  HeadPose pose;
  FacePoints pts{};
  cv::Rect2f box;
  if (best >= 0) {
    const double sx = double(frame.cols) / small.cols, sy = double(frame.rows) / small.rows;
    for (int k = 0; k < 5; ++k)
      pts[k] = { faces.at<float>(best, 4 + 2 * k) * sx, faces.at<float>(best, 5 + 2 * k) * sy };
    box = { float(faces.at<float>(best, 0) * sx), float(faces.at<float>(best, 1) * sy),
            float(faces.at<float>(best, 2) * sx), float(faces.at<float>(best, 3) * sy) };
    pose = solve_head_pose(pts, frame.size());
  }

  ++fps_count_;
  const std::chrono::duration<double> fps_elapsed = started - fps_start_;
  if (fps_elapsed.count() >= 1.0) {
    fps_ = fps_count_ / fps_elapsed.count();
    fps_count_ = 0;
    fps_start_ = started;
  }

  // 3. Centre, filter, joystick values
  bool centred = false;
  double yaw = 0, pitch = 0;
  int8_t x = 0, y = 0;

  if (pose.ok) {
    // Face back after the stick had returned to the centre: start the filters afresh
    const double lost = std::chrono::duration<double>(captured - last_face_).count();
    if (lost > settings_.lost_hold + settings_.lost_fade) {
      yaw_filter_.reset();
      pitch_filter_.reset();
    }
    last_face_ = captured;
    if (recentre_.exchange(false)) {
      sum_yaw_ = sum_pitch_ = 0;
      centre_count_ = 0;
    }
    if (centre_count_ < settings_.centre_frames) {
      // Collecting the centre position: average of the first frames
      sum_yaw_ += pose.yaw;
      sum_pitch_ += pose.pitch;
      if (++centre_count_ == settings_.centre_frames) {
        centre_yaw_ = sum_yaw_ / centre_count_;
        centre_pitch_ = sum_pitch_ / centre_count_;
        yaw_filter_.reset();
        pitch_filter_.reset();
        std::cout << "Head tracker: centre set (yaw " << centre_yaw_ << ", pitch " << centre_pitch_ << ")" << std::endl;
      }
    }
    else {
      centred = true;
      yaw = yaw_filter_.filter(pose.yaw - centre_yaw_, t);
      pitch = pitch_filter_.filter(pose.pitch - centre_pitch_, t);
      x = map_axis(yaw, settings_.x);
      y = map_axis(-pitch, settings_.y); // HID: negative Y is up, looking up gives positive pitch
      last_x_ = x;
      last_y_ = y;
    }
  }
  else if (centre_count_ >= settings_.centre_frames) {
    // Face lost: hold the last values, then return to the centre
    centred = true;
    const double lost = std::chrono::duration<double>(captured - last_face_).count();
    const double f = lost_face_factor(lost, settings_.lost_hold, settings_.lost_fade);
    x = static_cast<int8_t>(last_x_ * f);
    y = static_cast<int8_t>(last_y_ * f);
    yaw = yaw_filter_.value();
    pitch = pitch_filter_.value();
  }

  {
    std::lock_guard<std::mutex> lock(mtx_);
    ++state_.seq;
    state_.face = pose.ok;
    state_.centred = centred;
    state_.yaw = yaw;
    state_.pitch = pitch;
    state_.x = x;
    state_.y = y;
    state_.processing_ms = ms_since(started);
    state_.latency_ms = ms_since(captured);
    state_.fps = fps_;
    state_.frame_size = frame.size();
    state_.box = box;
    state_.points = pts;
  }
  updated_.notify_all();
}
