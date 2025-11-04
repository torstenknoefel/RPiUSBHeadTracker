#pragma once
#include <lccv.hpp>
#include <opencv2/opencv.hpp>

class RPiHTCam
{

public:

  RPiHTCam();
  ~RPiHTCam();


  uint32_t num_cams;
  uint32_t height;
  uint32_t width;


  cv::Mat image;
  lccv::PiCamera cam;



};
