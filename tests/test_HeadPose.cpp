#include <gtest/gtest.h>

#include <cmath>

#include <opencv2/calib3d.hpp>

#include "HeadPose.h"

namespace {

constexpr double kDeg = CV_PI / 180.0;
const cv::Size kImage(640, 480);

// Rotation of a head that turns right by 'yaw' and looks up by 'pitch' (degrees).
// Turning right: rotation about the camera's y axis (y points down);
// looking up: rotation about the x axis that moves the nose up (-y).
cv::Matx33d head_rotation(double yaw, double pitch)
{
  const double a = yaw * kDeg, b = -pitch * kDeg;
  const cv::Matx33d Ry(std::cos(a), 0, std::sin(a), 0, 1, 0, -std::sin(a), 0, std::cos(a));
  const cv::Matx33d Rx(1, 0, 0, 0, std::cos(b), -std::sin(b), 0, std::sin(b), std::cos(b));
  return Ry * Rx;
}

// The five face points as the camera would see a head 500 mm away,
// shifted sideways by (dx, dy) mm. The head turns about a point inside it,
// 90 mm behind the nose tip, like a real neck.
FacePoints project(double yaw, double pitch, double dx = 0, double dy = 0)
{
  const cv::Matx33d R = head_rotation(yaw, pitch);
  const cv::Matx33d K = camera_matrix(kImage);
  const cv::Vec3d pivot(0, 0, 90);
  FacePoints pts;
  for (size_t i = 0; i < 5; ++i) {
    const cv::Point3d& p = face_model()[i];
    const cv::Vec3d c = R * (cv::Vec3d(p.x, p.y, p.z) - pivot) + pivot + cv::Vec3d(dx, dy, 500);
    const cv::Vec3d uv = K * c;
    pts[i] = { uv[0] / uv[2], uv[1] / uv[2] };
  }
  return pts;
}

}  // namespace

TEST(HeadPose, StraightAheadGivesZero)
{
  const HeadPose p = solve_head_pose(project(0, 0), kImage);
  ASSERT_TRUE(p.ok);
  EXPECT_NEAR(p.yaw, 0, 0.5);
  EXPECT_NEAR(p.pitch, 0, 0.5);
}

TEST(HeadPose, RecoversKnownAngles)
{
  for (double yaw : { -30.0, -15.0, -5.0, 5.0, 15.0, 30.0 })
    for (double pitch : { -20.0, -10.0, 0.0, 10.0, 20.0 }) {
      const HeadPose p = solve_head_pose(project(yaw, pitch), kImage);
      ASSERT_TRUE(p.ok) << "yaw " << yaw << " pitch " << pitch;
      EXPECT_NEAR(p.yaw, yaw, 1.0) << "yaw " << yaw << " pitch " << pitch;
      EXPECT_NEAR(p.pitch, pitch, 1.0) << "yaw " << yaw << " pitch " << pitch;
    }
}

TEST(HeadPose, TurningRightMovesNoseToImageLeftAndGivesPositiveYaw)
{
  const FacePoints straight = project(0, 0), right = project(20, 0);
  EXPECT_LT(right[2].x, straight[2].x);  // nose tip moves to the image's left
  EXPECT_GT(solve_head_pose(right, kImage).yaw, 10);
}

TEST(HeadPose, LookingUpMovesNoseUpAndGivesPositivePitch)
{
  const FacePoints straight = project(0, 0), up = project(0, 15);
  EXPECT_LT(up[2].y, straight[2].y);  // nose tip moves up in the image
  EXPECT_GT(solve_head_pose(up, kImage).pitch, 8);
}

TEST(HeadPose, MovingSidewaysWithoutTurningChangesYawOnlyLittle)
{
  // Leaning 100 mm to the side changes the viewing angle by about 11 degrees,
  // but the head is not turned; most of it must not show up as yaw.
  const HeadPose p = solve_head_pose(project(0, 0, 100, 0), kImage);
  ASSERT_TRUE(p.ok);
  EXPECT_LT(std::fabs(p.yaw), 15.0);
}

TEST(HeadPose, WorksAtTheSmallDetectionSize)
{
  const cv::Size small(160, 120);
  FacePoints pts = project(10, -5);
  for (auto& p : pts) p *= 0.25;  // same view at a quarter of the size
  const HeadPose p = solve_head_pose(pts, small);
  ASSERT_TRUE(p.ok);
  EXPECT_NEAR(p.yaw, 10, 1.0);
  EXPECT_NEAR(p.pitch, -5, 1.0);
}
