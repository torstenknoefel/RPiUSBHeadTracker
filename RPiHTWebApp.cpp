#include <iomanip>
#include <mutex>
#include <regex>

#include "crow.h"

#include "RPiHTWebApp.h"
#include "QueryParser.h"
#include "TrackerSettings.h"

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

// Yes/no from the form ("true", "1", "on" or the field's own name); 'fallback' if the field is missing
static bool form_bool(QueryParser& post, const std::string& key, bool fallback)
{
  const std::string v = post.get(key);
  if (v.empty())
    return fallback;
  return v == "true" || v == "1" || v == "on" || v == key;
}

// Short text for a number in the form ("0.05", not "0.050000000000000003")
static std::string number_text(double v)
{
  std::ostringstream os;
  os << std::setprecision(6) << v;
  return os.str();
}

// Choices for the face detection size, as a list for the mustache template:
// value (width), text ("256 × 192 (default)") and "selected" for the current one.
// A width from the file that is not in the list is added, so it is not lost.
static crow::json::wvalue detect_options(const RPiHTConfigData& data)
{
  const cv::Size camera(int(data.cam_width), int(data.cam_height));
  // 320 is left out: there YuNet still finds "faces" on an empty wall
  std::vector<int> widths = { 160, 192, 224, 256 };
  if (std::find(widths.begin(), widths.end(), data.track_detect_width) == widths.end()) {
    widths.push_back(data.track_detect_width);
    std::sort(widths.begin(), widths.end());
  }
  std::vector<crow::json::wvalue> list;
  for (int w : widths) {
    const cv::Size s = detect_size_for(w, camera);
    std::string text = std::to_string(s.width) + " \xC3\x97 " + std::to_string(s.height);
    if (w == 160) text += " (fastest)";
    if (w == 256) text += " (default)";
    crow::json::wvalue o;
    o["value"] = w;
    o["text"] = text;
    o["selected"] = (w == data.track_detect_width) ? "selected" : "";
    list.push_back(std::move(o));
  }
  return crow::json::wvalue(list);
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
    : config_(config), tracker_(tracker)
  {
    running_ = config_.get_data();  // the program has just started with these values
    if (!config_.read_saved(saved_))
      saved_ = running_;

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

    // Saved / in use, for the status bar on every page
    CROW_ROUTE(webapp, "/status")
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          return status_response();
        });

    // Head tracking values from the Config page: used right away, no restart
    CROW_ROUTE(webapp, "/tracking").methods("POST"_method)
      ([this](const crow::request& req)
        {
          QueryParser post = QueryParser(req.body);
          std::lock_guard<std::mutex> lock(mtx_);
          RPiHTConfigData t = config_.get_data();
          t.track_range_x = form_number(post, "track_range_x", t.track_range_x);
          t.track_range_y = form_number(post, "track_range_y", t.track_range_y);
          t.track_deadzone = form_number(post, "track_deadzone", t.track_deadzone);
          t.track_curve = form_number(post, "track_curve", t.track_curve);
          t.track_smoothing = form_number(post, "track_smoothing", t.track_smoothing);
          t.track_speed = form_number(post, "track_speed", t.track_speed);
          t.track_invert_x = form_bool(post, "track_invert_x", t.track_invert_x);
          t.track_invert_y = form_bool(post, "track_invert_y", t.track_invert_y);
          t.track_detect_width = int(form_number(post, "track_detect_width", t.track_detect_width));
          config_.set_tracking(t);
          use_tracking();
          return status_response();
        });

    // Writes the current values to the config file
    CROW_ROUTE(webapp, "/save").methods("POST"_method)
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          config_.save();
          if (!config_.read_saved(saved_))
            return crow::response(500, "Could not write the config file");
          return status_response();
        });

    // Back to the values in the config file. Head tracking values change right
    // away; if camera, web server or joystick values differ, the program restarts.
    CROW_ROUTE(webapp, "/revert").methods("POST"_method)
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          if (!config_.reload())
            return crow::response(500, "Could not read the config file");
          saved_ = config_.get_data();
          if (config_.get_data().same_system(running_)) {
            use_tracking();
            return status_response();
          }
          reload_config = true;
          return status_response(true);
        });

    // Restarts camera, web server and joystick with the current values
    CROW_ROUTE(webapp, "/restart").methods("POST"_method)
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          reload_config = true;
          return status_response(true);
        });

    CROW_ROUTE(webapp, "/")
      ([]()
    {
      auto page = crow::mustache::load("status.html");
      crow::mustache::context ctx({ {"title","Status"}});
      return page.render(ctx);
    });

    CROW_ROUTE(webapp, "/adjust")
      ([]()
        {
          auto page = crow::mustache::load("adjust.html");
          crow::mustache::context ctx({ {"title", "Adjust"}});
          return page.render(ctx);
        });

    // Config page. The form posts only camera, web server and joystick values
    // ("Set" = restart with them, "Save" = write them to the file); head tracking
    // values come through /tracking.
    CROW_ROUTE(webapp, "/config")
      .methods("POST"_method, "GET"_method) ([this](const crow::request& req)
    {
      QueryParser post = QueryParser(req.body);
      std::lock_guard<std::mutex> lock(mtx_);
      if (post.keys().size() > 0) // we are getting a valid response
      {
        if (post.get("action") == "Save" || post.get("action") == "Set")
        {
          RPiHTConfigData tmp_config = config_.get_data();  // keeps the head tracking values
          tmp_config.device_path = post.get("device");
          tmp_config.ip = post.get("ip");
          tmp_config.port = stoi(post.get("port"));
          tmp_config.cam_height = stoi(post.get("cam_height"));
          tmp_config.cam_width = stoi(post.get("cam_width"));
          tmp_config.cam_framerate = stof(post.get("cam_framerate"));
          tmp_config.cam_verbose = (post.get("cam_verbose") == "cam_verbose");

          config_.set_data(tmp_config); // marks the config unsaved/inactive only if something changed
          if (post.get("action") == "Save")
          {
            config_.save();
            config_.read_saved(saved_);
          }
          else { // must be action "Set"
            reload_config = true;
          }
        }
      }
      const auto& data = config_.get_data();
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
      ctx["track_detect_options"] = detect_options(data);
      if (reload_config)
        ctx["restart_instance"] = instance_;  // the status bar shows the restart
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

    // Signals are handled in main() so the whole app shuts down, not only the web server
    _a = webapp.signal_clear().bindaddr(config.get_ip()).port(config.get_data().port).run_async();
  };
  ~Impl()
  {
    webapp.stop();
  };
private:
  // The current head tracking values go to the running tracker. Caller holds mtx_.
  void use_tracking()
  {
    running_.copy_tracking(config_.get_data());
    tracker_.update_settings(tracker_settings(running_));
  }

  // { saved, active, restarting, instance } for the status bar. Caller holds mtx_.
  //   saved:      the current values are the ones in the config file
  //   active:     the program runs with the current values (false: a restart is needed)
  //   restarting: a restart was just started
  //   instance:   changes with every restart, so the page sees when the restart is done
  crow::response status_response(bool restarting = false)
  {
    const auto& data = config_.get_data();
    crow::json::wvalue j;
    j["saved"] = (data == saved_);
    j["active"] = (data == running_);
    j["restarting"] = restarting;
    j["instance"] = instance_;
    auto response = crow::response(j);
    response.set_header("Cache-Control", "no-store");
    return response;
  }

  RPiHTConfig& config_;
  HeadTracker& tracker_;
  std::mutex mtx_;            // guards the config and the two copies below
  RPiHTConfigData running_;   // values the program runs with
  RPiHTConfigData saved_;     // values in the config file
  // Start time of this web app in ms: a new value after every restart
  const int64_t instance_ = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  crow::SimpleApp webapp;
  std::future<void> _a;
};


RPiHTWebApp::RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam, HeadTracker &tracker)
  : pImpl(std::make_unique<Impl>(config, cam, tracker))
{};

RPiHTWebApp::~RPiHTWebApp() = default;
