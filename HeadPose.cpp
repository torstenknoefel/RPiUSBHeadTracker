#include "HeadPose.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <opencv2/calib3d.hpp>

const std::array<cv::Point3d, 5>& face_model()
{
  static const std::array<cv::Point3d, 5> model = { {
    {-31, -35, 30},  // right eye (image left)
    { 31, -35, 30},  // left eye
    {  0,   0,  0},  // nose tip
    {-25,  32, 20},  // right mouth corner
    { 25,  32, 20},  // left mouth corner
  } };
  return model;
}

cv::Matx33d camera_matrix(cv::Size image_size)
{
  const double hfov = 62.2 * CV_PI / 180.0;
  const double f = (image_size.width / 2.0) / std::tan(hfov / 2.0);
  return { f, 0, image_size.width / 2.0,
           0, f, image_size.height / 2.0,
           0, 0, 1 };
}

HeadPose solve_head_pose(const FacePoints& points, cv::Size image_size)
{
  const auto& model = face_model();
  std::vector<cv::Point3d> object(model.begin(), model.end());
  std::vector<cv::Point2d> image(points.begin(), points.end());

  cv::Mat rvec, tvec;
  HeadPose pose;
  if (!cv::solvePnP(object, image, cv::Mat(camera_matrix(image_size)), cv::noArray(), rvec, tvec,
                    false, cv::SOLVEPNP_SQPNP))
    return pose;

  // Direction the face points to: the model's "towards the camera" axis (0, 0, -1),
  // rotated into camera coordinates.
  cv::Matx33d R;
  cv::Rodrigues(rvec, R);
  const cv::Vec3d f = R * cv::Vec3d(0, 0, -1);

  // Turning right moves the face direction to the image's left (-x);
  // looking up moves it up (-y).
  pose.yaw = std::atan2(-f[0], -f[2]) * 180.0 / CV_PI;
  pose.pitch = std::atan2(-f[1], std::hypot(f[0], f[2])) * 180.0 / CV_PI;
  pose.ok = std::isfinite(pose.yaw) && std::isfinite(pose.pitch);
  return pose;
}

cv::Size detect_size_for(int width, cv::Size camera)
{
  if (camera.width <= 0 || camera.height <= 0)
    camera = { 640, 480 };
  const int w = std::clamp(width, 96, std::max(camera.width, 96));
  const int h = std::max(int(std::lround(double(w) * camera.height / camera.width)), 32);
  return { w, h };
}

bool plausible_face_box(const cv::Rect2f& box, cv::Size frame)
{
  return box.width <= 0.8f * frame.width && box.height <= 1.0f * frame.height;
}
