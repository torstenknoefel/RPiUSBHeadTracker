#!/bin/bash
# Starts the head tracker automatically at boot (systemd), and again after a crash.
#
#   sudo bash install_service.sh [config file] [build folder]
#   sudo bash install_service.sh --remove
#
# Defaults: config file /etc/USBHT.json, build folder ./build next to this script.
# The program runs as the user who called sudo.

set -e

if [ "$(id -u)" -ne 0 ]; then
  echo "Please run as root: sudo bash $0" >&2
  exit 1
fi

UNITS="headtracker.service headtracker-gadget.service"

if [ "$1" = "--remove" ]; then
  systemctl disable --now $UNITS 2>/dev/null || true
  for u in $UNITS; do rm -f "/etc/systemd/system/$u"; done
  systemctl daemon-reload
  echo "Removed. The head tracker no longer starts at boot."
  exit 0
fi

SOURCE="$(cd "$(dirname "$0")" && pwd)"
CONFIG="$(realpath -m "${1:-/etc/USBHT.json}")"
BUILD="$(realpath -m "${2:-$SOURCE/build}")"
RUN_USER="${SUDO_USER:-}"

if [ -z "$RUN_USER" ] || [ "$RUN_USER" = "root" ]; then
  echo "Please start this script with sudo from the user the program should run as." >&2
  exit 1
fi
if [ ! -x "$BUILD/RPiUSBHeadTracker" ]; then
  echo "Program not found: $BUILD/RPiUSBHeadTracker - build it first (see README)." >&2
  exit 1
fi
if [ ! -f "$CONFIG" ]; then
  echo "Config file not found: $CONFIG" >&2
  exit 1
fi
if ! sudo -u "$RUN_USER" test -w "$CONFIG"; then
  echo "Note: $RUN_USER cannot write $CONFIG - Save in the web interface will not work." >&2
fi

for u in $UNITS; do
  sed -e "s|@USER@|$RUN_USER|g" -e "s|@SOURCE@|$SOURCE|g" \
      -e "s|@BUILD@|$BUILD|g" -e "s|@CONFIG@|$CONFIG|g" -e 's/\r$//' \
      "$SOURCE/service/$u.in" > "/etc/systemd/system/$u"
done

systemctl daemon-reload
systemctl enable $UNITS
# A head tracker started by hand would hold the camera
pkill -x RPiUSBHeadTrack 2>/dev/null && sleep 2 || true
systemctl restart headtracker-gadget.service headtracker.service

echo "Installed: the head tracker starts at boot and again after a crash."
echo "  Status:  systemctl status headtracker"
echo "  Log:     journalctl -u headtracker -f"
echo "  Stop:    sudo systemctl stop headtracker"
