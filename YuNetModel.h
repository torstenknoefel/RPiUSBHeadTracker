#pragma once
#include <opencv2/core/version.hpp>

// File name of the YuNet face model in the models folder. OpenCV 4.8 changed
// how cv::FaceDetectorYN reads the network's output: up to 4.7 (bookworm: 4.6)
// only the 2022 model works, from 4.8 (trixie: 4.10) only the 2023 one.
// CMakeLists.txt downloads the same file by the same rule.
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 8)
inline constexpr const char* yunet_model_file = "face_detection_yunet_2023mar.onnx";
#else
inline constexpr const char* yunet_model_file = "face_detection_yunet_2022mar.onnx";
#endif
