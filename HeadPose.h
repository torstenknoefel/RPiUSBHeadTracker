#pragma once
#include <array>

#include <opencv2/core.hpp>

// Head angles from the five face points YuNet returns.
//
// Conventions (degrees, seen from the person in front of the camera):
//   yaw   > 0: the person turns the head to their right
//   pitch > 0: the person looks up
// The camera image is not mirrored, so the person's right is on the image's left.

struct HeadPose
{
  bool ok = false;
  double yaw = 0;
  double pitch = 0;
};

// YuNet landmark order: right eye, left eye, nose tip, right mouth corner, left mouth corner
using FacePoints = std::array<cv::Point2d, 5>;

// Average face (mm), camera axes: x right, y down, z away from the camera.
// Origin at the nose tip; eyes and mouth lie behind it.
const std::array<cv::Point3d, 5>& face_model();

// Camera matrix for the IMX219 (Camera Module v2), full field of view
// (horizontal about 62.2 degrees), for an image of the given size.
cv::Matx33d camera_matrix(cv::Size image_size);

// Solves the head rotation from the five points (pixel coordinates in an
// image of 'image_size'). Returns ok = false if the solver fails.
HeadPose solve_head_pose(const FacePoints& points, cv::Size image_size);
