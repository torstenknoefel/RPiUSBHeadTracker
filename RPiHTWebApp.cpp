#include <regex>

#include "crow.h"

#include "RPiHTWebApp.h"
#include "QueryParser.h"

extern std::atomic_bool run;
extern std::atomic_bool reload_config;

bool stob(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  std::istringstream is(str);
  bool b;
  is >> std::boolalpha >> b;
  return b;
}

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
      crow::mustache::context ctx({ {"title","Status"}}); //
      return page.render(ctx);
    });

    // Capture config by reference explicitly in the outer lambda
    CROW_ROUTE(webapp, "/adjust")
      ([&config]()
        {
          auto page = crow::mustache::load("adjust.html");
          crow::mustache::context ctx({ {"title", "Adjust"}}); //
          return page.render(ctx);
        });

    // Capture config by reference explicitly in the outer lambda
    CROW_ROUTE(webapp, "/config")
      .methods("POST"_method, "GET"_method) ([&config](const crow::request& req)
    {
      QueryParser post = QueryParser(req.body);
      if (post.keys().size() > 0) // we are getting a valid response
      {
        if (post.get("action") == "Save" ||  post.get("action") == "Set")
          config.data.device_path = post.get("device");
          config.data.ip = post.get("ip");
          config.data.port = stoi(post.get("port"));
          config.data.cam_height = stoi(post.get("cam_height"));
          config.data.cam_width = stoi(post.get("cam_width"));
          config.data.cam_framerate = stof(post.get("cam_framerate"));
          config.data.cam_verbose = stob(post.get("cam_verbose"));
          if (post.get("action") == "Save")
          {
            config.save();
          }
          else { // must be Set
            reload_config = true;
          }
      }
      auto page = crow::mustache::load("config.html");
      crow::mustache::context ctx({ {"title", "Configuration"},
                                    {"device", config.data.device_path},
                                    {"ip", config.data.ip},
                                    {"port", config.data.port},
                                    {"cam_height", config.data.cam_height},
                                    {"cam_width", config.data.cam_width},
                                    {"cam_framerate", config.data.cam_framerate},
                                    {"cam_verbose", config.data.cam_verbose},
                                  });
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
    //CROW_ROUTE(webapp, "/<string>")
    //  ([](std::string name)
    //{
    //  auto page = crow::mustache::load("fancypage.html"); //
    //  crow::mustache::context ctx({ {"person", name} }); //
    //  return page.render(ctx); //
    //});

    _a = webapp.bindaddr(config.data.ip).port(config.data.port).run_async();
  };
  ~Impl()
  {
    webapp.stop();
  };
private:
  crow::SimpleApp webapp;
  std::future<void> _a;
  cv::Mat image;
};


RPiHTWebApp::RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam) : pImpl(std::make_unique<Impl>(config, cam))
{};

RPiHTWebApp::~RPiHTWebApp() = default;
