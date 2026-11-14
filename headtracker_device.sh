#!/bin/bash
# Creates the USB HID joystick gadget (/dev/hidg0) via configfs.
# Run as root: sudo ./headtracker_device.sh
# Safe to run again: an already active gadget is left alone.

set -e

if [ "$(id -u)" -ne 0 ]; then
  echo "Please run as root: sudo $0" >&2
  exit 1
fi

modprobe libcomposite

GADGET=/sys/kernel/config/usb_gadget/rpi_headtracker

if [ -n "$(cat "$GADGET/UDC" 2>/dev/null)" ]; then
  echo "Head tracker gadget is already active"
  exit 0
fi

# Create xac_joystick gadget
mkdir -p "$GADGET"
cd "$GADGET"

# Define USB specification
echo 0x1d6b > idVendor  # Linux Foundation
echo 0x0104 > idProduct # Multifunction Composite Joystick Gadget
echo 0x0100 > bcdDevice # v1.0.0
echo 0x0200 > bcdUSB    # USB2
echo 0x02   > bDeviceClass
echo 0x00   > bDeviceSubClass
echo 0x00   > bDeviceProtocol

# Device information
mkdir -p strings/0x409
date +%s > strings/0x409/serialnumber
echo "Torsten Knoefel" > strings/0x409/manufacturer
echo "RaspberryPi Head Tracker" > strings/0x409/product

# Create configuration file
mkdir -p configs/c.1/strings/0x409
echo 0x80 > configs/c.1/bmAttributes
echo 250  > configs/c.1/MaxPower # 250 mA
echo "Joystick configuration" > configs/c.1/strings/0x409/configuration

# Define the functions of the device
mkdir -p functions/hid.usb0
echo 0 > functions/hid.usb0/protocol
echo 0 > functions/hid.usb0/subclass
# One report: X and Y (1 byte each) + 32 buttons (4 bytes) = 6 bytes, see USBJoystick.cpp
echo 6 > functions/hid.usb0/report_length

# HID descriptor for a joystick with 32 buttons
echo -ne \\x05\\x01\\x09\\x04\\xA1\\x01\\x15\\x81\\x25\\x7F\\x09\\x01\\xA1\\x00\\x09\\x30\\x09\\x31\\x75\\x08\\x95\\x02\\x81\\x02\\xC0\\xA1\\x00\\x05\\x09\\x19\\x01\\x29\\x20\\x15\\x00\\x25\\x01\\x75\\x01\\x95\\x20\\x81\\x02\\xC0\\xC0 > functions/hid.usb0/report_desc
####                                                                                                                                                                        ^^ Max buttons                          ^^ Reported buttons

# Link the configuration file
[ -e configs/c.1/hid.usb0 ] || ln -s functions/hid.usb0 configs/c.1

# Activate device
ls /sys/class/udc | head -n 1 > UDC

# Let the user who called sudo write reports without root
if [ -n "$SUDO_USER" ]; then
  for _ in $(seq 50); do [ -e /dev/hidg0 ] && break; sleep 0.1; done
  chown "$SUDO_USER" /dev/hidg0
fi

sleep 10
