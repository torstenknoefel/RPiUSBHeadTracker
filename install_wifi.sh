#!/bin/bash
# Wi-Fi setup without a terminal: when the Pi is on no Wi-Fi network, it opens
# its own open Wi-Fi "HeadTracker-XXXX". A phone that joins it gets the setup
# page, where the user picks the home network and types its password.
#
#   sudo bash install_wifi.sh [config file]
#   sudo bash install_wifi.sh --remove
#
# Needs install_service.sh first (the head tracker as a service). Default config
# file: /etc/USBHT.json (for the web server's port). The program's user (the one
# who called sudo) may then change the Wi-Fi through NetworkManager.
#
# Careful: on a Pi that is reached over its Wi-Fi, a missing or wrong network
# makes the Pi open the hotspot instead - it is then reachable only there.

set -e

if [ "$(id -u)" -ne 0 ]; then
  echo "Please run as root: sudo bash $0" >&2
  exit 1
fi

UNITS="headtracker-wifi.service headtracker-portal.socket headtracker-portal.service"
# 10-: polkit uses the first rule that decides, in name order, and the
# system's 50-default.rules would ask for an admin password first
POLKIT=/etc/polkit-1/rules.d/10-headtracker-wifi.rules
CAPTIVE=/etc/NetworkManager/dnsmasq-shared.d/headtracker-captive.conf

if [ "$1" = "--remove" ]; then
  systemctl disable --now $UNITS 2>/dev/null || true
  for u in $UNITS; do rm -f "/etc/systemd/system/$u"; done
  rm -f "$POLKIT" "$CAPTIVE"
  systemctl daemon-reload
  nmcli connection delete id headtracker-hotspot >/dev/null 2>&1 || true
  echo "Removed. The Pi no longer opens its own Wi-Fi."
  exit 0
fi

SOURCE="$(cd "$(dirname "$0")" && pwd)"
CONFIG="$(realpath -m "${1:-/etc/USBHT.json}")"
RUN_USER="${SUDO_USER:-}"

if [ -z "$RUN_USER" ] || [ "$RUN_USER" = "root" ]; then
  echo "Please start this script with sudo from the user the program runs as." >&2
  exit 1
fi
if ! command -v nmcli >/dev/null; then
  echo "NetworkManager (nmcli) not found - it is part of Raspberry Pi OS since Bookworm." >&2
  exit 1
fi
PORT=$(grep -o '"port": *[0-9]*' "$CONFIG" 2>/dev/null | grep -o '[0-9]*$' || true)
if [ -z "$PORT" ]; then
  echo "No web server port found in $CONFIG" >&2
  exit 1
fi

for u in $UNITS; do
  sed -e "s|@USER@|$RUN_USER|g" -e "s|@SOURCE@|$SOURCE|g" -e "s|@PORT@|$PORT|g" -e 's/\r$//' \
      "$SOURCE/service/$u.in" > "/etc/systemd/system/$u"
done
sed -e "s|@USER@|$RUN_USER|g" -e 's/\r$//' "$SOURCE/wifi/headtracker-wifi.rules.in" > "$POLKIT"
mkdir -p "$(dirname "$CAPTIVE")"
sed -e 's/\r$//' "$SOURCE/wifi/headtracker-captive.conf" > "$CAPTIVE"

systemctl daemon-reload
systemctl enable headtracker-wifi.service headtracker-portal.socket
systemctl restart headtracker-portal.socket headtracker-wifi.service

echo "Installed: without a Wi-Fi network the Pi opens its own Wi-Fi \"HeadTracker-...\"."
echo "  Setup page on the hotspot: http://10.42.0.1/setup (opens by itself on most phones)"
echo "  Log:    journalctl -u headtracker-wifi -f"
echo "  Remove: sudo bash install_wifi.sh --remove"
