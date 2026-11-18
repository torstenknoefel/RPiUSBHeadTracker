#!/bin/bash -e
# Installs the head tracker packages into the image. image/build.sh puts the
# packages built by the Package workflow (rpi-headtracker, rpi-headtracker-wifi,
# +deb13) into files/. Their install scripts create the user "headtracker",
# enable the services and add the USB device mode to config.txt; nothing starts
# here (no systemd in the build).

install -d "${ROOTFS_DIR}/tmp/headtracker-debs"
install -m 644 files/*.deb "${ROOTFS_DIR}/tmp/headtracker-debs/"

on_chroot << CHROOT
apt-get install -y /tmp/headtracker-debs/*.deb
rm -rf /tmp/headtracker-debs
CHROOT

# Check the result, so a broken image fails the build instead of the user
grep -q '^dtoverlay=dwc2,dr_mode=peripheral' "${ROOTFS_DIR}/boot/firmware/config.txt"
test -L "${ROOTFS_DIR}/etc/systemd/system/multi-user.target.wants/rpi-headtracker.service"
test -L "${ROOTFS_DIR}/etc/systemd/system/multi-user.target.wants/rpi-headtracker-wifi.service"
