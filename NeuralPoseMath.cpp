#include "NeuralPoseMath.h"

#include <algorithm>
#include <cmath>

#include <opencv2/calib3d.hpp>

static double degrees(double rad) { return rad * 180.0 / CV_PI; }

HeadPose head_pose_from_quaternion(const cv::Vec4f& q_xyzw, cv::Point2d face_centre, cv::Size image_size)
{
  HeadPose pose;
  double x = q_xyzw[0], y = q_xyzw[1], z = q_xyzw[2], w = q_xyzw[3];
  const double n = std::sqrt(x * x + y * y + z * z + w * w);
  if (!(n > 1e-6))
    return pose;
  x /= n; y /= n; z /= n; w /= n;
  const cv::Matx33d r(1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
                      2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
                      2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y));

  // Rotation that turns the camera axis (0, 0, 1) onto the line of sight to the
  // face: what the network calls "straight into the camera" is along that line.
  const cv::Matx33d k = camera_matrix(image_size);
  cv::Vec3d ray((face_centre.x - k(0, 2)) / k(0, 0), (face_centre.y - k(1, 2)) / k(1, 1), 1.0);
  ray /= cv::norm(ray);
  const cv::Vec3d axis = cv::Vec3d(0, 0, 1).cross(ray);
  const double s = cv::norm(axis);
  cv::Matx33d correction = cv::Matx33d::eye();
  if (s > 1e-9) {
    const cv::Vec3d rvec = axis / s * std::atan2(s, ray[2]);
    cv::Rodrigues(rvec, correction);
  }

  // Direction the face looks in (no rotation: towards the camera, -z)
  const cv::Vec3d look = correction * r * cv::Vec3d(0, 0, -1);
  pose.yaw = degrees(std::atan2(-look[0], -look[2]));                       // > 0: the person turns to their right
  pose.pitch = degrees(std::atan2(-look[1], std::hypot(look[0], look[2]))); // > 0: looking up
  pose.ok = std::isfinite(pose.yaw) && std::isfinite(pose.pitch);
  return pose;
}

void normalize_face_crop(const cv::Mat& crop, cv::Mat& out)
{
  CV_Assert(crop.type() == CV_8UC1);
  int histogram[256] = {};
  for (int r = 0; r < crop.rows; ++r) {
    const uchar* p = crop.ptr<uchar>(r);
    for (int c = 0; c < crop.cols; ++c)
      ++histogram[p[c]];
  }
  // Grey level below which 90 % of the pixels lie
  const double wanted = crop.total() * 0.9;
  int level = 0;
  for (double count = 0; level < 255; ++level) {
    count += histogram[level];
    if (count > wanted)
      break;
  }
  const double scale = level < 127 ? 0.9 * 0.5 / std::max(5, level) : 1.0 / 255;
  crop.convertTo(out, CV_32F, scale, -0.5);
}

float box_overlap(const cv::Rect2f& a, const cv::Rect2f& b)
{
  const float inter = (a & b).area();
  const float uni = a.area() + b.area() - inter;
  return uni > 0 ? inter / uni : 0.f;
}

double rotation_uncertainty_deg(const float* tril)
{
  double worst = 0;
  for (int i = 0; i < 3; ++i) {
    // diagonal of L * L^T: sum of squares of row i (L is lower triangular)
    double v = 0;
    for (int j = 0; j <= i; ++j)
      v += double(tril[3 * i + j]) * tril[3 * i + j];
    worst = std::max(worst, std::sqrt(v));
  }
  return degrees(worst);
}
