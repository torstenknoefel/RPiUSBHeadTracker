#pragma once

#include <opencv2/core.hpp>

#include "HeadPose.h"

// Helpers for the NeuralNet face model (see NeuralPose.h) that need no
// ONNX Runtime, so the unit tests can check them.

// Head angles (conventions as in HeadPose.h) from the rotation the pose network
// returns: a quaternion (x, y, z, w) in image axes (x right, y down, z into the
// picture; no rotation = the face looks straight into the camera).
// The network sees only the face crop, as if the face were in the middle of the
// picture. 'face_centre' (pixels in an image of 'image_size') corrects for the
// face being off-centre: a face at the edge of the picture that looks into the
// camera is turned, seen from the camera's axis.
HeadPose head_pose_from_quaternion(const cv::Vec4f& q_xyzw, cv::Point2d face_centre, cv::Size image_size);

// Network input from a greyscale face crop (8 bit): brightness stretched so that
// the brightest 10 % of the pixels reach the top of the range, values around 0
// (about -0.5 ... 0.5), 32-bit float.
void normalize_face_crop(const cv::Mat& crop, cv::Mat& out);

// Overlap of two boxes: intersection / union, 0 ... 1
float box_overlap(const cv::Rect2f& a, const cv::Rect2f& b);

// Largest standard deviation (degrees) of the rotation the network is unsure
// about, from the lower triangular factor L (3 x 3, row by row, radians) of the
// covariance L * L^T it returns.
double rotation_uncertainty_deg(const float* tril);
