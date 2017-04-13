#!/bin/bash
module="fourmb_device_driver"
device="fourmb_device_driver"
mode="664"
major=61

# Compile and load the device
make
rm -rf /dev/${device}	# remove stale devices
mknod /dev/${device} c $major 0
insmod ./$module.ko
