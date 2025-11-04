
#include <stdlib.h>

#include <stdio.h>
#include <iostream>

//#include <lccv.hpp>
#include <libcamera_app.hpp>
//#include <opencv2/opencv.hpp>

#include "RPiHTCam.h"


RPiHTCam::RPiHTCam()
{
  num_cams = LibcameraApp::GetNumberCameras();
  std::cout << "Found " << num_cams << " cameras." << std::endl;

  height = 480;
  width = 640;
  
  image = cv::Mat(height, width, CV_8UC3);

  cam.options->video_width = width;
  cam.options->video_height = height;
  cam.options->framerate = 30;
  cam.options->verbose = true;
 // cv::namedWindow("Video", cv::WINDOW_NORMAL);
  cam.startVideo();


 // int ch = 0;
 // while (ch != 27) {
 //   if (!cam.getVideoFrame(image, 1000)) {
 //     std::cout << "Timeout error" << std::endl;
 //   }
 //   else {
 //     cv::imshow("Video1", image);
 //   }
 //   ch = cv::waitKey(5);
 // }
}

RPiHTCam::~RPiHTCam()
{
  cam.stopVideo();
  cv::destroyAllWindows();
}