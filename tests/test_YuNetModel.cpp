#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <opencv2/objdetect.hpp>

#include "YuNetModel.h"

// The YuNet model downloaded by CMake must be one this OpenCV can run: with the
// wrong one (2022 model on OpenCV 4.8+, 2023 model before) detect() throws.
TEST(YuNetModel, LoadsAndRunsWithThisOpenCV)
{
  const std::string path = std::string(RPIHT_MODEL_DIR) + "/" + yunet_model_file;
  ASSERT_TRUE(std::filesystem::exists(path)) << path << " missing (downloaded by CMake)";

  const cv::Size size(256, 192); // TrackerSettings::detect_size
  cv::Ptr<cv::FaceDetectorYN> detector;
  ASSERT_NO_THROW(detector = cv::FaceDetectorYN::create(path, "", size, 0.6f, 0.3f, 5));
  ASSERT_FALSE(detector.empty());

  cv::Mat image(size, CV_8UC3, cv::Scalar(90, 110, 130)), faces;
  ASSERT_NO_THROW(detector->detect(image, faces)) << "OpenCV " << CV_VERSION << " cannot run " << yunet_model_file;
  // Box, five points and score per face. (Plain pictures can give a "face":
  // the tracker leaves those out with plausible_face_box.)
  EXPECT_TRUE(faces.empty() || faces.cols == 15) << faces.cols << " columns";
}
