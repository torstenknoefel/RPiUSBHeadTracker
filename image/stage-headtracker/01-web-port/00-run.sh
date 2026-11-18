#!/bin/bash -e
# The web page on port 80 everywhere, so it is at http://headtracker.local
# (the program itself listens on 18080). The Wi-Fi package's portal socket only
# listens on the hotspot's address; here it listens on all addresses, and its
# relay passes everything on to the program's port.

install -d "${ROOTFS_DIR}/etc/systemd/system/rpi-headtracker-portal.socket.d"
cat > "${ROOTFS_DIR}/etc/systemd/system/rpi-headtracker-portal.socket.d/image.conf" << CONF
# SD card image: port 80 on all addresses, not only on the hotspot (10.42.0.1)
[Socket]
ListenStream=
ListenStream=80
CONF
