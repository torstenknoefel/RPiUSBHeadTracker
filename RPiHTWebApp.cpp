#include <iomanip>
#include <mutex>
#include <regex>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "crow.h"

#include "RPiHTWebApp.h"
#include "NeuralPose.h"
#include "QueryParser.h"
#include "TrackerSettings.h"
#include "WifiSetup.h"

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

// Choices for the face model, as a list for the mustache template. NeuralNet is
// shown but cannot be chosen when the program was built without it.
static crow::json::wvalue model_options(const RPiHTConfigData& data)
{
  struct Choice { const char* value; std::string text; bool usable; };
  const Choice choices[] = {
    { "yunet", "YuNet \xE2\x80\x93 fast, weaker up/down (default)", true },
    { "neuralnet", NeuralPose::built_in() ? "NeuralNet \xE2\x80\x93 precise up/down, more CPU"
                                          : "NeuralNet (not built in)", NeuralPose::built_in() },
  };
  std::vector<crow::json::wvalue> list;
  for (const auto& c : choices) {
    crow::json::wvalue o;
    o["value"] = c.value;
    o["text"] = c.text;
    o["selected"] = data.track_model == c.value ? "selected" : "";
    o["disabled"] = c.usable ? "" : "disabled";
    list.push_back(std::move(o));
  }
  return crow::json::wvalue(list);
}

// true if a TCP connection to the web server's own address and port succeeds
static bool port_answers(const std::string& ip, int port)
{
  const std::string host = (ip.empty() || ip == "0.0.0.0") ? "127.0.0.1" : ip;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (port <= 0 || port > 65535 || inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    return false;
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  const bool ok = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
  ::close(fd);
  return ok;
}

#ifndef RPIHT_VERSION
#define RPIHT_VERSION "unknown"  // set by CMake from project(VERSION)
#endif

// When the program started, for the uptime on the Help page (the web app
// itself starts again with every restart)
static const auto program_start = std::chrono::steady_clock::now();

// Name of this computer ("dacia")
static std::string host_name()
{
  char name[256] = {};
  return gethostname(name, sizeof(name) - 1) == 0 ? std::string(name) : std::string();
}

// IPv4 addresses of this computer's network connections that are up,
// without the loopback address
static std::vector<std::string> own_addresses()
{
  std::vector<std::string> list;
  ifaddrs* all = nullptr;
  if (getifaddrs(&all) != 0)
    return list;
  for (const ifaddrs* a = all; a != nullptr; a = a->ifa_next) {
    if (a->ifa_addr == nullptr || a->ifa_addr->sa_family != AF_INET
        || (a->ifa_flags & IFF_LOOPBACK) || !(a->ifa_flags & IFF_UP))
      continue;
    char text[INET_ADDRSTRLEN] = {};
    const auto* in = reinterpret_cast<const sockaddr_in*>(a->ifa_addr);
    if (inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text)) != nullptr)
      list.emplace_back(text);
  }
  freeifaddrs(all);
  return list;
}

// The Wi-Fi. One for the whole program: the web app starts again with every
// restart, a network being joined must not be lost then.
static WifiSetup& wifi_setup()
{
  static WifiSetup wifi;
  return wifi;
}

// Address of the setup page on the hotspot (see wifi/headtracker-wifi.sh)
static const char* const hotspot_setup_url = "http://10.42.0.1/setup";

// Wi-Fi state, nearby networks and the last join as JSON for the setup page
static crow::json::wvalue wifi_json(bool with_networks)
{
  auto& wifi = wifi_setup();
  const WifiStatus s = wifi.status();
  crow::json::wvalue j;
  j["available"] = s.available;
  j["mode"] = s.mode;
  j["ssid"] = s.ssid;
  j["address"] = s.address;
  j["hostname"] = host_name();
  const WifiJoin join = wifi.last_join();
  j["join"]["ssid"] = join.ssid;
  j["join"]["busy"] = join.busy;
  j["join"]["ok"] = join.ok;
  j["join"]["message"] = join.message;
  j["join"]["age_s"] = join.age_s;
  if (with_networks) {
    crow::json::wvalue::list list;
    for (const auto& n : wifi.networks()) {
      crow::json::wvalue o;
      o["ssid"] = n.ssid;
      o["signal"] = n.signal;
      o["security"] = n.security;
      o["open"] = n.open();
      o["usable"] = wifi::key_mgmt_for(n.security) != "unsupported";
      list.push_back(std::move(o));
    }
    j["networks"] = std::move(list);
  }
  return j;
}

// Tracker state as JSON for the web pages
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
  j["model"] = s.model;
  j["frame"]["width"] = s.frame_size.width;
  j["frame"]["height"] = s.frame_size.height;
  if (s.face) {
    j["box"] = crow::json::wvalue::list({ s.box.x, s.box.y, s.box.width, s.box.height });
    crow::json::wvalue::list points;
    if (s.has_points)
      for (const auto& p : s.points)
        points.push_back(crow::json::wvalue::list({ p.x, p.y }));
    j["points"] = std::move(points);
  }
  return j;
}

class RPiHTWebApp::Impl
{
public:
  Impl(RPiHTConfig &config, RPiHTCam &cam, HeadTracker &tracker, Trial &trial)
    : config_(config), tracker_(tracker), trial_(trial)
  {
    running_ = config_.get_data();  // the program has just started with these values
    if (!config_.read_saved(saved_))
      saved_ = running_;

    // Tracker state for the web pages (polled several times per second)
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

    // Trying new camera / connection settings: keep them, go back, or hide the
    // message about a trial that was undone
    CROW_ROUTE(webapp, "/keep").methods("POST"_method)
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          trial_.keep();
          return status_response();
        });
    CROW_ROUTE(webapp, "/undo").methods("POST"_method)
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          if (!trial_.active())
            return status_response();
          config_.set_data(trial_.cancel());
          reload_config = true;
          return status_response(true);
        });
    CROW_ROUTE(webapp, "/dismiss").methods("POST"_method)
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          trial_.dismiss();
          return status_response();
        });

    // Head tracking values from the Config and Adjust pages: used right away, no
    // restart. Each page sends only its own values; the others stay as they are.
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
          const std::string model = post.get("track_model");
          if (model == "yunet" || (model == "neuralnet" && NeuralPose::built_in()))
            t.track_model = model;
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

    // Restarts camera, web server and joystick with the current values,
    // as a trial if they differ from the running ones
    CROW_ROUTE(webapp, "/restart").methods("POST"_method)
      ([this]()
        {
          std::lock_guard<std::mutex> lock(mtx_);
          restart();
          return status_response(true);
        });

    CROW_ROUTE(webapp, "/")
      ([]()
    {
      auto page = crow::mustache::load("status.html");
      crow::mustache::context ctx({ {"title","Status"}});
      return page.render(ctx);
    });

    // Adjust page: head tracking values (live, through /tracking)
    CROW_ROUTE(webapp, "/adjust")
      .methods("POST"_method, "GET"_method) ([this](const crow::request& req)
        {
          return settings_page(req, "adjust.html", "Adjust");
        });

    // Config page: camera picture, face model and detection size (live, through
    // /tracking), camera, joystick device and web server values (used after a
    // restart, posted here)
    CROW_ROUTE(webapp, "/config")
      .methods("POST"_method, "GET"_method) ([this](const crow::request& req)
        {
          return settings_page(req, "config.html", "Configuration");
        });

    // Help page; its "About this tracker" part loads /info
    CROW_ROUTE(webapp, "/help")
      ([this]()
        {
          auto page = crow::mustache::load("help.html");
          crow::mustache::context ctx({ {"title", "Help"} });
          ctx["trial_seconds"] = int(trial_.timeout().count());
          ctx["hostname"] = host_name();
          {
            std::lock_guard<std::mutex> lock(mtx_);
            ctx["port"] = running_.port;
          }
          return crow::response(page.render(ctx));
        });

    // Facts about this tracker for the Help page
    CROW_ROUTE(webapp, "/info")
      ([this, &cam]()
        {
          crow::json::wvalue j;
          {
            std::lock_guard<std::mutex> lock(mtx_);
            j["port"] = running_.port;
            j["device"] = running_.device_path;
            j["camera"]["width"] = running_.cam_width;
            j["camera"]["height"] = running_.cam_height;
            j["camera"]["framerate"] = running_.cam_framerate;
            j["model"] = running_.track_model;
          }
          j["version"] = RPIHT_VERSION;
          j["hostname"] = host_name();
          crow::json::wvalue::list addresses;
          for (const auto& a : own_addresses())
            addresses.push_back(a);
          j["addresses"] = std::move(addresses);
          j["config_file"] = config_.file_path();
          j["neuralnet_built_in"] = NeuralPose::built_in();
          j["camera_fps"] = cam.fps();
          j["uptime_s"] = int64_t(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - program_start).count());
          j["opencv"] = CV_VERSION;
          auto response = crow::response(j);
          response.set_header("Cache-Control", "no-store");
          return response;
        });

    // Wi-Fi setup page: on the hotspot the first page a phone sees
    CROW_ROUTE(webapp, "/setup")
      ([this]()
        {
          auto page = crow::mustache::load("setup.html");
          crow::mustache::context ctx({ {"title", "Wi-Fi"} });
          ctx["hostname"] = host_name();
          {
            std::lock_guard<std::mutex> lock(mtx_);
            ctx["port"] = running_.port;  // the page's address once the Pi is on the home network
          }
          return crow::response(page.render(ctx));
        });

    // Wi-Fi state; with ?networks=1 also the nearby networks (takes a few seconds)
    CROW_ROUTE(webapp, "/wifi")
      ([](const crow::request& req)
        {
          auto response = crow::response(wifi_json(req.url_params.get("networks") != nullptr));
          response.set_header("Cache-Control", "no-store");
          return response;
        });

    // Joins a network (form: ssid, password); the answer comes before it is joined
    CROW_ROUTE(webapp, "/wifi/join").methods("POST"_method)
      ([](const crow::request& req)
        {
          QueryParser post(req.body);
          const bool started = wifi_setup().join(post.get("ssid"), post.get("password"));
          crow::json::wvalue j;
          j["started"] = started;
          return crow::response(j);
        });

    // Forgets the network the Pi is on; without another one the hotspot opens
    CROW_ROUTE(webapp, "/wifi/forget").methods("POST"_method)
      ([]()
        {
          crow::json::wvalue j;
          j["ok"] = wifi_setup().forget();
          return crow::response(j);
        });

    // Any other address. A phone on the hotspot checks for internet with
    // addresses of other names ("connectivitycheck.gstatic.com/generate_204"):
    // it is sent to the setup page, so it opens that page by itself.
    CROW_CATCHALL_ROUTE(webapp)
      ([](const crow::request& req, crow::response& res)
        {
          if (!wifi::own_host_name(req.get_header_value("Host"), host_name())) {
            res.code = 302;
            res.set_header("Location", hotspot_setup_url);
            res.set_header("Cache-Control", "no-store");
          }
          else {
            res.code = 404;
            res.body = "Not found";
          }
          res.end();
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
    // Crow reports the server as started even if it could not bind the port,
    // so check that the port really answers
    webapp.wait_for_server_start(std::chrono::seconds(3));
    started_ = port_answers(config.get_ip(), config.get_data().port);
  };

  bool started() const { return started_; }

  void trial_expired()
  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!trial_.active())
      return;
    config_.set_data(trial_.fail("The new settings were not confirmed within "
      + std::to_string(trial_.timeout().count()) + " seconds."));
    reload_config = true;
  }
  ~Impl()
  {
    webapp.stop();
  };
private:
  // The Adjust and Config pages. Their restart forms post only their own values
  // ("Try" = restart with them as a trial, "Save" = write them to the file):
  // "section" says which ones; an older page without it sends all of them.
  crow::response settings_page(const crow::request& req, const char* file, const char* title)
  {
    QueryParser post = QueryParser(req.body);
    std::lock_guard<std::mutex> lock(mtx_);
    std::string trial_url;  // where the page finds the program after a port change
    const std::string action = post.get("action");
    if (action == "Save" || action == "Try")
    {
      const std::string section = post.get("section");
      RPiHTConfigData t = config_.get_data();  // keeps the values the form does not send
      if (section.empty() || section == "camera") {
        t.cam_width = int(form_number(post, "cam_width", t.cam_width));
        t.cam_height = int(form_number(post, "cam_height", t.cam_height));
        t.cam_framerate = form_number(post, "cam_framerate", t.cam_framerate);
        t.cam_verbose = (post.get("cam_verbose") == "cam_verbose");  // not sent when unticked
      }
      if (section.empty() || section == "connection") {
        t.device_path = post.get("device", t.device_path);
        t.ip = post.get("ip", t.ip);
        t.port = int(form_number(post, "port", t.port));
      }

      config_.set_data(t); // marks the config unsaved/inactive only if something changed
      if (action == "Save")
      {
        config_.save();
        config_.read_saved(saved_);
      }
      else { // "Try"
        restart();
        if (config_.get_data().port != running_.port)
          trial_url = new_url(req, config_.get_data().port);
      }
    }
    const auto& data = config_.get_data();
    auto page = crow::mustache::load(file);
    crow::mustache::context ctx({ {"title", title},
                                  {"device", data.device_path},
                                  {"ip", data.ip},
                                  {"port", data.port},
                                  {"cam_height", data.cam_height},
                                  {"cam_width", data.cam_width},
                                  {"cam_framerate", number_text(data.cam_framerate)},
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
    ctx["track_model_options"] = model_options(data);
    if (reload_config)
      ctx["restart_instance"] = instance_;  // the status bar shows the restart
    ctx["trial_url"] = trial_url;
    ctx["trial_seconds"] = int(trial_.timeout().count());
    return crow::response(page.render(ctx));
  }

  // Restart with the current values; a trial if camera, web server or joystick
  // values differ from the running ones. Caller holds mtx_.
  void restart()
  {
    if (!config_.get_data().same_system(running_))
      trial_.start(running_);
    reload_config = true;
  }

  // Address of this page with another port ("http://dacia:18081")
  static std::string new_url(const crow::request& req, int port)
  {
    std::string host = req.get_header_value("Host");
    const auto colon = host.rfind(':');
    if (colon != std::string::npos && host.find(']', colon) == std::string::npos)
      host.erase(colon);
    return "http://" + host + ":" + std::to_string(port);
  }

  // The current head tracking values go to the running tracker. Caller holds mtx_.
  void use_tracking()
  {
    running_.copy_tracking(config_.get_data());
    tracker_.update_settings(tracker_settings(running_));
  }

  // { saved, active, restarting, instance, trial, failure } for the status bar. Caller holds mtx_.
  //   saved:      the current values are the ones in the config file
  //   active:     the program runs with the current values (false: a restart is needed)
  //   restarting: a restart was just started
  //   instance:   changes with every restart, so the page sees when the restart is done
  //   trial:      seconds until the last working settings come back (-1: no trial)
  //   failure:    why the last trial was undone ("" if it was not)
  crow::response status_response(bool restarting = false)
  {
    const auto& data = config_.get_data();
    crow::json::wvalue j;
    j["saved"] = (data == saved_);
    j["active"] = (data == running_);
    j["restarting"] = restarting;
    j["instance"] = instance_;
    j["trial"] = trial_.seconds_left();
    j["failure"] = trial_.failure();
    auto response = crow::response(j);
    response.set_header("Cache-Control", "no-store");
    // After a port change the page still comes from the old port and asks here
    response.set_header("Access-Control-Allow-Origin", "*");
    return response;
  }

  RPiHTConfig& config_;
  HeadTracker& tracker_;
  Trial& trial_;
  bool started_ = false;
  std::mutex mtx_;            // guards the config and the two copies below
  RPiHTConfigData running_;   // values the program runs with
  RPiHTConfigData saved_;     // values in the config file
  // Start time of this web app in ms: a new value after every restart
  const int64_t instance_ = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  crow::SimpleApp webapp;
  std::future<void> _a;
};


RPiHTWebApp::RPiHTWebApp(RPiHTConfig &config, RPiHTCam &cam, HeadTracker &tracker, Trial &trial)
  : pImpl(std::make_unique<Impl>(config, cam, tracker, trial))
{};

bool RPiHTWebApp::started() const { return pImpl->started(); }

void RPiHTWebApp::trial_expired() { pImpl->trial_expired(); }

RPiHTWebApp::~RPiHTWebApp() = default;
