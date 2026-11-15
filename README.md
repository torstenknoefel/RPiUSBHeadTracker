# RPiUSBHeadTracker

A Raspberry Pi head tracker that a PC sees as an ordinary USB joystick.

The Pi is plugged into the PC with a USB cable. It registers as a USB game controller, so games and simulators that can use a joystick need no extra driver or software. A camera on the Pi watches the player, and the program turns the camera picture into joystick movements. A small web interface lets you check the camera picture and change the settings from any browser.

> **Status: work in progress.** Head tracking works: turning the head moves X, tilting it moves Y. Sensitivity, dead zone and smoothing are set on the Config page; the Status page shows the tracker live and has a Centre button. The program does not start automatically at boot yet.

## How it works

```
 camera ──► RPiUSBHeadTracker ──► /dev/hidg0 ──► USB cable ──► PC: "HID-compliant game controller"
                   │
                   └──► web interface (port 18080) ──► browser on PC or phone
```

- **USB joystick:** `headtracker_device.sh` turns the Pi's USB port into a HID joystick (Linux USB gadget). The program writes joystick reports to `/dev/hidg0`: two axes (X, Y, each −127 … 127) and 32 buttons.
- **Camera:** read through libcamera and OpenCV (via the LCCV library), in its own thread.
- **Head tracking:** for every camera frame, a small face detection model (YuNet) finds the face and five points on it (eyes, nose tip, mouth corners). From these points the program works out how far the head is turned (yaw) and tilted (pitch), smooths the values and turns them into joystick X and Y. See [Head tracking](#head-tracking).
- **Web interface:** a small built-in web server (Crow) with three pages: Status, Adjust and Config.

## What you need

- **Raspberry Pi 4** (or another Pi whose USB‑C port can work as a USB device, such as the Pi Zero 2 W). Tested on a Pi 4 with Raspberry Pi OS / Debian 12 "bookworm".
- **Raspberry Pi camera**, tested with the Camera Module v2 (IMX219).
- **USB cable** from the Pi's USB‑C port to the PC. The Pi is also powered through this cable.

## Setup on the Pi

### 1. Let the USB port work as a device

Add this line to `/boot/firmware/config.txt` and reboot:

```
dtoverlay=dwc2,dr_mode=peripheral
```

### 2. Install the build tools and libraries

```bash
sudo apt install cmake ninja-build g++ libopencv-dev libcamera-dev libssl-dev libasio-dev
```

CMake 3.24 or newer is needed (bookworm has 3.25).

Optional, for the unit tests:

```bash
sudo apt install libgtest-dev
```

### 3. Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

The first `cmake` run needs an internet connection: it downloads the libraries listed under [Libraries](#libraries) into `build/_deps`, and the face detection model into `build/models`. Later builds in the same build folder use these copies.

### 4. Create the USB joystick

```bash
sudo bash headtracker_device.sh
```

The PC now finds a new game controller called "RaspberryPi Head Tracker". When the script is started with `sudo`, it gives `/dev/hidg0` to your user, so the program can run without root. You can run the script again at any time; if the joystick is already set up, it does nothing. The setup is lost on reboot, so run it again after each start.

### 5. Create the config file

The program reads its settings from `/etc/USBHT.json`, or from the file given with `-c`. Example:

```json
{
   "device": "/dev/hidg0",
   "ip": "0.0.0.0",
   "port": 18080,
   "cam_width": 640,
   "cam_height": 480,
   "cam_framerate": 80.0,
   "cam_verbose": false
}
```

| Key | Meaning |
|---|---|
| `device` | The joystick device created in step 4, normally `/dev/hidg0`. |
| `ip` | Address the web interface listens on. `0.0.0.0` means all network connections of the Pi. Optional. |
| `port` | Port of the web interface. |
| `cam_width`, `cam_height` | Camera picture size in pixels. |
| `cam_framerate` | Camera frames per second. |
| `cam_verbose` | `true` writes detailed camera messages to the log. |
| `track_range_x`, `track_range_y` | Degrees of head turn (X) or tilt (Y) for full joystick deflection. Optional, default 20 for X and 10 for Y. |
| `track_deadzone` | Degrees around the centre that give 0. Optional, default 1.5. |
| `track_curve` | 1 = linear; above 1 finer near the centre and faster towards the edge. Optional, default 1. |
| `track_smoothing` | Smoothing while the head is still, in Hz: lower = calmer, but more lag. Optional, default 1. |
| `track_speed` | How quickly the smoothing lets fast movements through: higher = less lag. Optional, default 0.05. |
| `track_invert_x`, `track_invert_y` | `true` reverses the direction of X or Y. Optional, default `false`. |
| `track_detect_width` | Width in pixels the camera picture is shrunk to for face detection; the height keeps the camera's aspect ratio. Larger = steadier, above all up/down, but slower. Optional, default 256. |

When you press **Save** in the web interface, the program writes this file itself and adds the keys `saved` and `active`. Your user needs write permission for the file.

### 6. Start

Start the program from the build folder, because it loads the web pages from the `templates` and `static` folders there:

```bash
cd build
./RPiUSBHeadTracker -c ~/USBHT.json
```

| Option | Meaning |
|---|---|
| `-c`, `--configfile` | Full path to the config file. Default: `/etc/USBHT.json`. |
| `-h`, `--help` | Show the options. |

Stop it with **Ctrl+C**, or `pkill -x RPiUSBHeadTrack` from another terminal. The program then closes the camera, the web server and the joystick device cleanly. It does not start automatically at boot yet.

## The web interface

Open `http://<address of the Pi>:18080` in a browser on the PC or on a phone in the same network. The menu on the left switches between the pages. On a phone the menu moves to the bottom of the screen.

### Status bar

Every page starts with a bar that shows whether the settings are saved and in use:

| Bar | Meaning | Buttons |
|---|---|---|
| Green: **Settings saved and in use** | The program runs with the values from the config file. | – |
| Amber: **Changed settings in use – not saved** | You changed head tracking values. They are lost when the program restarts. | **Save** writes them to the file. **Back to saved** returns to the values in the file. |
| Amber: **Saved, but not in use yet** | Camera or connection values were saved but need a restart. | **Restart now** |
| Blue: **Restarting with the new settings …** | Camera, web server and joystick start again. | – |
| Red: **No connection to the head tracker** | The program is not running or the network is down. | – |

### Status

![Status page](docs/images/status.jpg)

The start page shows the tracker live, about five times per second:

- whether a face is found and the centre position is set ("Tracking", "No face found", "Setting the centre …"),
- the joystick position as a dot in a square,
- head turn and tilt in degrees, joystick X and Y,
- camera and tracker frames per second, processing time and the time from camera frame to joystick values.

**Centre:** look straight at the screen and press it; the head position of the next frames becomes "straight ahead". The program also does this by itself after every start.

The same data is available as JSON at `/state`; `POST /centre` does what the button does. The status bar uses `/status` (`saved`, `active`); `POST /tracking` (head tracking values as form fields), `POST /save`, `POST /revert` and `POST /restart` do what the Config page and the status bar buttons do.

### Adjust

![Adjust page with the live camera picture and the face found by the tracker](docs/images/adjust.jpg)

Shows the live camera picture, updated about ten times per second. Use it to aim the camera at your head. A green box and five red points show where the tracker finds the face, eyes, nose tip and mouth corners. Below the picture are the tracker state and the Centre button. The picture is also available on its own at `/image`.

### Config

![Config page](docs/images/config.jpg)

The page has two parts.

**Head tracking:** every change is used right away; nothing restarts and the centre position stays. Two graphs show how head turn (left/right) and head tilt (up/down) become joystick values: the blue line is the current setting, the grey band the dead zone, and the red dot shows where your head is now and what the joystick sends. Move a slider and the line changes at once.

| Setting | Meaning |
|---|---|
| **Range left/right**, **Range up/down** | Head turn or tilt (degrees) for full joystick deflection. Smaller = more sensitive. |
| **Dead zone** | Movements this small (degrees) around the centre give 0. |
| **Curve** | 1 = straight line. Above 1: finer near the centre, faster towards the edge. |
| **Smoothing** | Lower = calmer when holding still, but more lag. |
| **Follow speed** | Higher = less lag on fast head movements. |
| **Invert left/right**, **Invert up/down** | Reverse the direction of X or Y. |
| **Face detection size** | Size the camera picture is shrunk to for finding the face. Larger = steadier, above all up/down, but more processing time (shown on the Status page). |

**Defaults** sets all head tracking values back to the program's defaults. Changed values are not saved until you press **Save** in the status bar; **Back to saved** there returns to the values in the file.

**Camera and connection:** these need a restart.

| Field | Meaning |
|---|---|
| **Camera width**, **Camera height** | Camera picture size in pixels. |
| **Camera frames per second** | Camera frames per second. |
| **Detailed camera log** | Writes detailed camera messages to the log. |
| **Joystick device** (Advanced) | The USB joystick device, normally `/dev/hidg0`. |
| **Web server address**, **Web server port** (Advanced) | Address and port the web interface listens on (`0.0.0.0` = all). After a port change, open the web interface at the new port. |

- **Undo edits** puts these fields back to the values the page showed when it was loaded.
- **Restart with these** restarts camera, web server and joystick with the new values. They are not saved yet; the status bar shows that.
- **Save** writes all values to the config file. Camera and connection values are used after the next restart.

On a phone the form uses the whole width and the menu is at the bottom:

<img src="docs/images/config-phone.jpg" alt="Config page on a phone" width="300">

### Help

The Help entry in the menu has no page yet.

## Checking the joystick on the PC

On Windows, press **Win+R**, type `joy.cpl` and press Enter. "RaspberryPi Head Tracker" appears in the list of game controllers. Open **Properties** to watch the axes move: turning your head moves the crosshair left and right, tilting it moves the crosshair up and down.

On Linux, `jstest /dev/input/js0` (package `joystick`) shows the same.

## Head tracking

- **Centre position:** after the start, and when you press **Centre** on the Status or Adjust page, the program averages the head position over the next 15 frames with a face and uses it as "straight ahead". The log shows `Head tracker: centre set …`.
- **Range:** by default 20° of head turn or 10° of head tilt gives full joystick deflection. Tilt has the smaller range because the five face points show a nod less clearly than a turn, so nods measure smaller than they are. Small movements within 1.5° of the centre give 0 (dead zone), so the view stays still when you hold your head still. Both can be changed on the Config page and are used right away.
- **Smoothing:** a "1€ filter" smooths strongly while the head is still (no jitter) and hardly at all during fast movements (little lag).
- **Face lost:** the joystick holds its last values for 0.5 s, then returns to the centre within 1 s.
- **Face detection size:** the camera picture is shrunk before the face is searched. The default 256 × 192 keeps the face points steady; at 160 × 120 the up/down value jitters about five times as much when you hold still, but processing is faster. Change it on the Config page.
- **False faces:** a "face" box wider than 80 % of the picture or taller than the whole picture is ignored. At some sizes the face model reports such boxes on an empty wall or in a dark room.
- **Speed:** on a Pi 4 at 256 × 192 processing takes about 20 ms per frame, so the tracker keeps up with the camera at 30 frames per second. At 160 × 120 it takes about 9 ms, enough for 60 frames per second.
- **Log:** every 10 s the program writes a line like `Camera: 30.0 frames/s | Tracker: 30.0 frames/s, face 100%, processing 20.5 ms, latency 21.0 ms …`.

The face model (YuNet, 2022 version, MIT licence) comes from the [OpenCV model zoo](https://github.com/opencv/opencv_zoo). CMake downloads it at configure time and checks it against its SHA256. The newer 2023 version of the model needs OpenCV 4.8; Debian bookworm has 4.6.

## Unit tests

The parts that need no camera, web server or USB connection have GoogleTest unit tests: the form parser, the config file, the joystick reports, the head angles, the smoothing filter and the conversion to joystick values. They are built automatically when `libgtest-dev` is installed:

```bash
ninja -C build
ctest --test-dir build
```

To build without tests, add `-DRPIHT_BUILD_TESTS=OFF` to the `cmake` command.

## Troubleshooting

| Problem | Solution |
|---|---|
| `Permission denied` on `/dev/hidg0` | Run `headtracker_device.sh` with `sudo` from your own user, or run `sudo chown $USER /dev/hidg0`. |
| No game controller on the PC | Check the `dtoverlay=dwc2,dr_mode=peripheral` line, use the Pi's USB‑C port and a data cable (not a charge-only cable), and run `headtracker_device.sh` again. |
| `$'\r': command not found` when running the script | The script has Windows line endings. Check it out again with this repository's `.gitattributes`, or run `sed -i 's/\r$//' headtracker_device.sh`. |
| Web interface does not open | Check the port in the config file and that the program is running. The log shows `Crow/master server is running at …`. |
| No camera picture | Check the camera cable and that `rpicam-hello --list-cameras` (or `libcamera-hello --list-cameras` on older systems) finds the camera. |

## Project layout

| Path | Content |
|---|---|
| `RPiUSBHeadTracker.cpp` | Main program and main loop. |
| `USBJoystick.*` | Writes the joystick reports to `/dev/hidg0`. |
| `RPiHTCam.*` | Camera access; reads every frame in its own thread. |
| `HeadTracker.*` | Finds the face in each frame and produces the joystick values. |
| `TrackerSettings.*` | Tracker settings from the config values. |
| `HeadPose.*` | Head angles from the five face points. |
| `OneEuroFilter.h` | Smoothing filter. |
| `JoystickMapping.*` | Head angle → joystick value (range, dead zone, curve). |
| `RPiHTWebApp.*` | Web interface and its routes. |
| `RPiHTConfig.*` | Reads and writes the config file. |
| `QueryParser.*` | Reads the values sent by the Config form. |
| `templates/`, `static/` | Web pages and style sheets. |
| `headtracker_device.sh` | Creates the USB joystick. |
| `tests/` | Unit tests. |

## Libraries

CMake downloads these from GitHub (`FetchContent` in `CMakeLists.txt`). Each one is pinned to a fixed commit and checked against its SHA256, so every build uses exactly the same code.

Each library is downloaded from a fork under [github.com/torstenknoefel](https://github.com/torstenknoefel) first, and from the original repository only if that fails. The forks keep the pinned commits available even if an original repository is deleted or its history is rewritten. To update a library, click "Sync fork" on its fork, then change the commit and hash in `CMakeLists.txt`.

| Library | Used for | Pinned commit |
|---|---|---|
| [Crow](https://github.com/CrowCpp/Crow) | Web server | `6ba7d7f` (master, 2025-10-10) |
| [taocpp/config](https://github.com/taocpp/config) | Reading the config file | `93a455e` (main, 2024-04-13) |
| [taocpp/json](https://github.com/taocpp/json) | JSON, used by config | `11a31e1` (main, 2025-03-11) |
| [taocpp/PEGTL](https://github.com/taocpp/PEGTL) | Parser, used by json | `54a2e32` (main, 2025-05-18) |
| [LCCV](https://github.com/kbarni/LCCV) | Camera access (libcamera + OpenCV); built as a static library and linked into the program | `bca9b3d` (main, 2025-06-12) |
