#pragma once

#include <opencv2/core.hpp>

#include "JoystickMapping.h"
#include "RPiHTConfig.h"

// Settings of the head tracker. All of them can change while the tracker runs
// (HeadTracker::update_settings), without restarting the camera.
enum class FaceModel
{
  YuNet,     // face detector with five points; angles from the points (HeadPose.h)
  NeuralNet  // angles straight from a neural network (NeuralPose.h)
};

struct TrackerSettings
{
  FaceModel model = FaceModel::YuNet;
  cv::Size detect_size{ 256, 192 }; // frames are shrunk to this size for YuNet
  float min_score = 0.6f;           // YuNet confidence for a face
  int centre_frames = 15;           // frames averaged for the centre position
  AxisMapping x{};                  // head turn (yaw)   -> X
  AxisMapping y{};                  // head tilt (pitch) -> Y
  double filter_min_cutoff = 1.0;   // OneEuroFilter, see OneEuroFilter.h
  double filter_beta = 0.05;
  double lost_hold = 0.5;           // s the last value is held when the face is lost
  double lost_fade = 1.0;           // s to return to the centre after that
};

// Tracker settings from the config, with lower limits so that odd values
// from the web form cannot stop the tracker.
TrackerSettings tracker_settings(const RPiHTConfigData& d);
