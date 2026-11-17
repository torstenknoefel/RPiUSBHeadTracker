#pragma once

#include <memory>
#include <string>

#include <opencv2/core.hpp>

// "NeuralNet" face model: head angles straight from a neural network, using the
// models of the opentrack NeuralNet tracker (by Michael Welter,
// https://github.com/opentrack/opentrack, ISC licence) with ONNX Runtime.
// Own implementation of the models' interface; no opentrack code.
//
// Two networks:
//   head-localizer.onnx       finds one face in the whole picture (box + score).
//                             Only used when there is no face to follow.
//   head-pose-0.5-small.onnx  gets a square crop around the face and returns the
//                             head rotation, a new face box (used for the next
//                             frame) and how unsure it is.
// Compared with YuNet's five points it measures nodding about four times larger
// (closer to the real movement) and is steadier, but it needs about 25 ms per
// frame on all four cores of a Pi 4.

struct NeuralFace
{
  bool ok = false;           // face found
  double yaw = 0;            // degrees, conventions as in HeadPose.h
  double pitch = 0;
  cv::Rect2f box;            // face box, pixels of the frame
  double uncertainty = 0;    // degrees (largest standard deviation of the rotation)
};

class NeuralPose
{
public:
  // true if the program was built with ONNX Runtime (CMake option RPIHT_NEURALNET)
  static bool built_in();

  // Loads the two models from 'model_dir'. 'threads': CPU cores used.
  explicit NeuralPose(const std::string& model_dir, int threads = 4);
  ~NeuralPose();
  NeuralPose(const NeuralPose&) = delete;
  NeuralPose& operator=(const NeuralPose&) = delete;

  // false if the models could not be loaded (the reason went to the log)
  bool ok() const;

  // Head angles for one camera frame (BGR or greyscale)
  NeuralFace process(const cv::Mat& frame);

  // Forget the face being followed; the next frame searches the whole picture
  void reset();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
