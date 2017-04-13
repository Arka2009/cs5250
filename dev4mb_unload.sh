#!/bin/bash
module="fourmb_device_driver"
device="fourmb_device_driver"
mode="664"
major=61

# do clean
rm -rf /dev/${device} #remove stale nodes
rmmod ./$module.ko
make clean
