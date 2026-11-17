#include <gtest/gtest.h>

#include <cmath>

#include "NeuralPoseMath.h"

namespace {

const cv::Size kImage(640, 480);
const cv::Point2d kMiddle(320, 240);

// Quaternion (x, y, z, w) for a rotation by 'deg' about an axis (image axes)
cv::Vec4f rotation(cv::Vec3d axis, double deg)
{
  axis /= cv::norm(axis);
  const double h = deg * CV_PI / 360.0;
  return cv::Vec4f(float(axis[0] * std::sin(h)), float(axis[1] * std::sin(h)),
                   float(axis[2] * std::sin(h)), float(std::cos(h)));
}

}  // namespace

TEST(NeuralPoseMath, NoRotationInTheMiddleIsStraightAhead)
{
  const HeadPose p = head_pose_from_quaternion(cv::Vec4f(0, 0, 0, 1), kMiddle, kImage);
  ASSERT_TRUE(p.ok);
  EXPECT_NEAR(p.yaw, 0, 1e-6);
  EXPECT_NEAR(p.pitch, 0, 1e-6);
}

TEST(NeuralPoseMath, TurnAboutTheDownAxisIsYaw)
{
  // y points down: a positive turn about it moves the face to the image's left,
  // i.e. the person turns to their right
  const HeadPose p = head_pose_from_quaternion(rotation({ 0, 1, 0 }, 20), kMiddle, kImage);
  ASSERT_TRUE(p.ok);
  EXPECT_NEAR(p.yaw, 20, 1e-4);
  EXPECT_NEAR(p.pitch, 0, 1e-4);
}

TEST(NeuralPoseMath, TurnAboutTheRightAxisIsPitch)
{
  // x points right: a positive turn about it tips the face down
  const HeadPose p = head_pose_from_quaternion(rotation({ 1, 0, 0 }, 15), kMiddle, kImage);
  ASSERT_TRUE(p.ok);
  EXPECT_NEAR(p.yaw, 0, 1e-4);
  EXPECT_NEAR(p.pitch, -15, 1e-4);
}

TEST(NeuralPoseMath, QuaternionNeedNotBeNormalised)
{
  const cv::Vec4f q = rotation({ 0, 1, 0 }, 20) * 3.f;
  EXPECT_NEAR(head_pose_from_quaternion(q, kMiddle, kImage).yaw, 20, 1e-4);
}

TEST(NeuralPoseMath, ZeroQuaternionFails)
{
  EXPECT_FALSE(head_pose_from_quaternion(cv::Vec4f(0, 0, 0, 0), kMiddle, kImage).ok);
}

TEST(NeuralPoseMath, FaceOffCentreLookingIntoTheCameraIsTurned)
{
  // A face on the image's right half that looks into the camera has turned to
  // its right by the angle between the camera axis and the line to the face
  const cv::Matx33d k = camera_matrix(kImage);
  const cv::Point2d right(320 + 160, 240);
  const HeadPose p = head_pose_from_quaternion(cv::Vec4f(0, 0, 0, 1), right, kImage);
  ASSERT_TRUE(p.ok);
  EXPECT_NEAR(p.yaw, std::atan(160 / k(0, 0)) * 180 / CV_PI, 1e-4);
  EXPECT_NEAR(p.pitch, 0, 1e-4);

  // Above the middle, looking into the camera: looking down
  const HeadPose up = head_pose_from_quaternion(cv::Vec4f(0, 0, 0, 1), cv::Point2d(320, 100), kImage);
  EXPECT_LT(up.pitch, -5);
}

TEST(NeuralPoseMath, NormaliseStretchesDarkCrops)
{
  cv::Mat out;
  // Dark crop: the brightest pixels (50) are stretched to 0.45 above the bottom
  normalize_face_crop(cv::Mat(10, 10, CV_8UC1, cv::Scalar(50)), out);
  ASSERT_EQ(out.type(), CV_32F);
  EXPECT_NEAR(out.at<float>(0, 0), 50 * 0.45 / 50 - 0.5, 1e-5);
  // Bright crop: plain 0 ... 255 -> -0.5 ... 0.5
  normalize_face_crop(cv::Mat(10, 10, CV_8UC1, cv::Scalar(200)), out);
  EXPECT_NEAR(out.at<float>(0, 0), 200.0 / 255 - 0.5, 1e-5);
  // Almost black: no division by (almost) zero
  normalize_face_crop(cv::Mat(10, 10, CV_8UC1, cv::Scalar(1)), out);
  EXPECT_NEAR(out.at<float>(0, 0), 1 * 0.45 / 5 - 0.5, 1e-5);
}

TEST(NeuralPoseMath, NormaliseUsesTheTop10Percent)
{
  // 91 pixels at 20, 9 at 100: more than 90 % lie at or below 20, so 20 sets the scale
  cv::Mat crop(10, 10, CV_8UC1, cv::Scalar(20));
  crop.row(9).setTo(100);
  crop.at<uchar>(9, 0) = 20;
  cv::Mat out;
  normalize_face_crop(crop, out);
  EXPECT_NEAR(out.at<float>(0, 0), 20 * 0.45 / 20 - 0.5, 1e-5);
}

TEST(NeuralPoseMath, BoxOverlap)
{
  const cv::Rect2f a(0, 0, 10, 10);
  EXPECT_FLOAT_EQ(box_overlap(a, a), 1.f);
  EXPECT_FLOAT_EQ(box_overlap(a, cv::Rect2f(20, 20, 5, 5)), 0.f);
  EXPECT_NEAR(box_overlap(a, cv::Rect2f(5, 0, 10, 10)), 50.0 / 150.0, 1e-6);
  EXPECT_FLOAT_EQ(box_overlap(cv::Rect2f(), cv::Rect2f()), 0.f);
}

TEST(NeuralPoseMath, RotationUncertainty)
{
  // L = diag(0.01, 0.05, 0.02) rad: the largest standard deviation is 0.05 rad
  const float diag[9] = { 0.01f, 0, 0, 0, 0.05f, 0, 0, 0, 0.02f };
  EXPECT_NEAR(rotation_uncertainty_deg(diag), 0.05 * 180 / CV_PI, 1e-4);
  // Off-diagonal parts count: row 3 = (0.03, 0.04, 0) -> sqrt(0.0025) = 0.05
  const float tril[9] = { 0.01f, 0, 0, 0, 0.01f, 0, 0.03f, 0.04f, 0 };
  EXPECT_NEAR(rotation_uncertainty_deg(tril), 0.05 * 180 / CV_PI, 1e-4);
}
