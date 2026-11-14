#include <iomanip>
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

// Number from the form; 'fallback' if the field is missing or not a number
// (an old page in the browser cache may not send the newer fields).
static double form_number(QueryParser& post, const std::string& key, double fallback)
{
  try {
    return std::stod(post.get(key));
  }
  catch (const std::exception&) {
    return fallback;
  }
}

// Short text for a number in the form ("0.05", not "0.050000000000000003")
static std::string number_text(double v)
{
  std::ostringstream os;
  os << std::setprecision(6) << v;
  return os.str();
}

// Tracker state as JSON for the Status and Adjust pages
static crow::json::wvalue state_json(const TrackerState& s, double camera_fps)
{
  crow::json::wvalue j;
  j["face"] = s.face;
  j["centred"] = s.centred;
  j["yaw"] = s.yaw;
  j["pitch"] = s.pitch;
  j["x"] = int(s.x);
  j["y"] = int(s.y);
  j["fps"] = s.fps;
  j["camera_fps"] = camera_fps;
  j["processing_ms"] = s.processing_ms;
  j["latency_ms"] = s.latency_ms;
  j["frame"]["width"] = s.frame_size.width;
  j["frame"]["height"] = s.frame_size.height;
  if (s.face) {
    j["box"] = crow::json::wvalue::list({ s.box.x, s.box.y, s.box.width, s.box.height });
    crow::json::wvalue::list points;
    for (const auto& p : s.points)
      points.push_back(crow::json::wvalue::list({ p.x, p.y }));
    j["points"] = std::move(points);
  }
  return j;
}

class RPiHTWebApp::Impl
{
public:
  Impl(RPiHTConfig &config, RPiHTCam &cam, HeadTracker &tracker)
  {
    // Tracker state for the Status and Adjust pages (polled several times per second)
    CROW_ROUTE(webapp, "/state")
      ([&cam, &tracker]()
        {
          auto response = crow::response(state_json(tracker.state(), cam.fps()));
          response.set_header("Cache-Control", "no-store");
          return response;
        });

    // Centre button: the next head position becomes "straight ahead"
    CROW_ROUTE(webapp, "/centre").methods("POST"_method)
      ([&tracker]()
        {
          tracker.recentre();
          return crow::response(204);
        });


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
        if (post.get("action") == "Save" || post.get("action") == "Set")
        {
          RPiHTConfigData tmp_config;
          tmp_config.device_path = post.get("device");
          tmp_config.ip = post.get("ip");
          tmp_config.port = stoi(post.get("port"));
          tmp_config.cam_height = stoi(post.get("cam_height"));
          tmp_config.cam_width = stoi(post.get("cam_width"));
          tmp_config.cam_framerate = stof(post.get("cam_framerate"));
          tmp_config.cam_verbose = (post.get("cam_verbose") == "cam_verbose");
          const auto& current = config.get_data();
          tmp_config.track_range_x = form_number(post, "track_range_x", current.track_range_x);
          tmp_config.track_range_y = form_number(post, "track_range_y", current.track_range_y);
          tmp_config.track_deadzone = form_number(post, "track_deadzone", current.track_deadzone);
          tmp_config.track_curve = form_number(post, "track_curve", current.track_curve);
          tmp_config.track_smoothing = form_number(post, "track_smoothing", current.track_smoothing);
          tmp_config.track_speed = form_number(post, "track_speed", current.track_speed);
          tmp_config.track_invert_x = (post.get("track_invert_x") == "track_invert_x");
          tmp_config.track_invert_y = (post.get("track_invert_y") == "track_invert_y");

          config.set_data(tmp_config); // marks the config unsaved/inactive only if something changed
          if (post.get("action") == "Save")
          {
            config.save();
          }
          else { // must be action "Set"
            reload_config = true;
          }
        }
      }
      const auto& data = config.get_data();
      auto page = crow::mustache::load("config.html");
      crow::mustache::context ctx({ {"title", "Configuration"},
                                    {"device", data.device_path},
                                    {"ip", data.ip},
                                    {"port", data.port},
                                    {"cam_height", data.cam_height},
                                    {"cam_width", data.cam_width},
                                    {"cam_framerate", data.cam_framerate},
                                    {"cam_verbose", data.cam_verbose ? "checked" : ""},
                                    {"track_range_x", number_text(data.track_range_x)},
                                    {"track_range_y", number_text(data.track_range_y)},
                                    {"track_deadzone", number_text(data.track_deadzone)},
                                    {"track_curve", number_text(data.track_curve)},
                                    {"track_smoothing", number_text(data.track_smoothing)},
                                    {"track_speed", number_text(data.track_speed)},
                                    {"track_invert_x", data.track_invert_x ? "checked" : ""},
                                    {"track_invert_y", data.track_invert_y ? "checked" : ""},
                                  });
      return page.render(ctx);
    });

    // Newest camera frame as JPEG. Takes a copy of the frame the camera thread
    // keeps, so it never takes frames away from the head tracker.
    CROW_ROUTE(webapp, "/image")
      ([&cam]()
        {
          cv::Mat image;
          if (!cam.latest(image))
            return crow::response(503, "No camera frame yet");
          std::vector<uchar> buff;
          std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 90 };
          cv::imencode(".jpg", image, buff, params);

          auto response = crow::response(std::string(buff.begin(), buff.end()));
          response.set_header("Content-Type", "image/jpeg");
          response.set_header("Cache-Control", "no-store");
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

    // Signals are handled in main() so the whole app shuts down, not only the web server
    _a = webapp.signal_clear().bindaddr(config.get_ip()).port(config.get_data().port).run_async();
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


RPiHTWebApp::RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam, HeadTracker &tracker)
  : pImpl(std::make_unique<Impl>(config, cam, tracker))
{};

RPiHTWebApp::~RPiHTWebApp() = default;
