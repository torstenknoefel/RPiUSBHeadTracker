#include <regex>

#include "crow.h"

#include "RPiHTWebApp.h"
#include "QueryParser.h"



class RPiHTWebApp::Impl
{
public:
  Impl(RPiHTConfig &config, RPiHTCam &cam)
  {


    // Capture config by reference explicitly in the outer lambda
    CROW_ROUTE(webapp, "/")
      ([&config]()
    {
      auto page = crow::mustache::load("status.html");
      crow::mustache::context ctx({ {"person", config.data.device_path} }); //
      return page.render(ctx);
    });

    // Capture config by reference explicitly in the outer lambda
    CROW_ROUTE(webapp, "/adjust")
      ([&config]()
        {
          auto page = crow::mustache::load("adjust.html");
          crow::mustache::context ctx({ {"person", config.data.device_path} }); //
          return page.render(ctx);
        });

    // Capture config by reference explicitly in the outer lambda
    CROW_ROUTE(webapp, "/config")
      .methods("POST"_method, "GET"_method) ([&config](const crow::request& req)
    {
      QueryParser post = QueryParser(req.body);
      if (post.keys().size() > 0) // we are getting a valid response
      {
        config.data.device_path = post.get("device");

        if (post.get("action") == "Save")
        {
          config.save();
        }
      }
      auto page = crow::mustache::load("config.html");
      crow::mustache::context ctx({ {"device", config.data.device_path} });
      return page.render(ctx);
    });

    // test CAM IMAGE
    CROW_ROUTE(webapp, "/image")
      ([&config,&cam]()
        {
          auto image = cv::Mat(cam.height, cam.width, CV_8UC3);
          cam.cam.getVideoFrame(image, 1000);
          std::vector<uchar> buff;
          std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 90 };
          cv::imencode(".jpg", image, buff, params);



          auto response = crow::response(std::string(buff.begin(), buff.end()));
          response.set_header("Content-Type", "application/octet-stream");
          return response;
        });

    // test
    CROW_ROUTE(webapp, "/<string>")
      ([](std::string name)
    {
      auto page = crow::mustache::load("fancypage.html"); //
      crow::mustache::context ctx({ {"person", name} }); //
      return page.render(ctx); //
    });

    _a = webapp.port(config.data.port).run_async();
  };
  ~Impl() = default;
private:
  crow::SimpleApp webapp;
  std::future<void> _a;
  cv::Mat image;
};


RPiHTWebApp::RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam) : pImpl(std::make_unique<Impl>(config, cam))
{};

RPiHTWebApp::~RPiHTWebApp() = default;
