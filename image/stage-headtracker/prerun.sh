#!/bin/bash -e
# Starts from the root file system of the stage before (stage2: Raspberry Pi OS Lite)

if [ ! -d "${ROOTFS_DIR}" ]; then
	copy_previous
fi
