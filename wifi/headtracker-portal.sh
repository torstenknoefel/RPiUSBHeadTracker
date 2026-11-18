#!/bin/sh
# Passes connections to port 80 of the hotspot (10.42.0.1) on to the head
# tracker's web server: phones check for internet on port 80, the web server
# listens on the port in its config file (default 18080). Started by
# rpi-headtracker-portal.socket (Debian package rpi-headtracker-wifi), which
# hands over the listening socket.
CONFIG="${HEADTRACKER_CONFIG:-/var/lib/rpi-headtracker/config.json}"
PORT=$(sed -n 's/.*"port"[[:space:]]*:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$CONFIG" 2>/dev/null | head -n 1)
exec /lib/systemd/systemd-socket-proxyd "127.0.0.1:${PORT:-18080}"
