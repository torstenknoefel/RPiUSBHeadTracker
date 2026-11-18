#!/bin/bash
# Builds the SD card image: Raspberry Pi OS Lite trixie 64-bit (pi-gen, the tool
# Raspberry Pi builds its own images with) plus the head tracker packages.
#
#   image/build.sh <folder with the .deb files> [output folder]
#
# The folder must hold rpi-headtracker_*+deb13_arm64.deb and
# rpi-headtracker-wifi_*+deb13_all.deb (from the Package workflow). Output:
# <date>-headtracker-<version>.img.xz (default folder: deploy/ next to image/).
#
# Needs Docker (pi-gen runs in a privileged container), git, about 15 GB of disk
# and an arm64 machine (on other machines pi-gen emulates arm64, much slower).
# GitHub Actions runs this in .github/workflows/package.yml.

set -euo pipefail

# pi-gen of the Raspberry Pi OS release 2026-10-06 (trixie, arm64), pinned to its commit
PIGEN_TAG=2026-10-06-raspios-trixie-arm64
PIGEN_COMMIT=b2cd98a9ee08a31ef7d0110ed09f2492bbd972ec

if [ $# -lt 1 ]; then
  echo "Usage: $0 <folder with the .deb files> [output folder]" >&2
  exit 1
fi
HERE="$(cd "$(dirname "$0")" && pwd)"
DEBS="$(realpath "$1")"
OUT="$(realpath -m "${2:-$HERE/../deploy}")"

CORE=$(ls "$DEBS"/rpi-headtracker_*+deb13_arm64.deb)
WIFI=$(ls "$DEBS"/rpi-headtracker-wifi_*+deb13_all.deb)
VERSION=$(basename "$CORE" | sed 's/^rpi-headtracker_\([^+_]*\).*/\1/')
echo "Packages: $(basename "$CORE"), $(basename "$WIFI") (version $VERSION)"

WORK="$(mktemp -d)"
# pi-gen's container leaves files owned by root behind
trap 'rm -rf "$WORK" 2>/dev/null || sudo rm -rf "$WORK"' EXIT
git clone --quiet --depth 1 --branch "$PIGEN_TAG" https://github.com/RPi-Distro/pi-gen.git "$WORK/pi-gen"
if [ "$(git -C "$WORK/pi-gen" rev-parse HEAD)" != "$PIGEN_COMMIT" ]; then
  echo "pi-gen tag $PIGEN_TAG is not at commit $PIGEN_COMMIT any more - stopping" >&2
  exit 1
fi

cd "$WORK/pi-gen"
cp -r "$HERE/stage-headtracker" .
mkdir -p stage-headtracker/00-install/files
cp "$CORE" "$WIFI" stage-headtracker/00-install/files/
touch stage2/SKIP_IMAGES  # only our stage writes an image
cp "$HERE/config" config
echo "IMG_NAME=headtracker-$VERSION" >> config

./build-docker.sh

mkdir -p "$OUT"
cp deploy/*.img.xz "$OUT/"
ls -l "$OUT"
