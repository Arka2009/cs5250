#!/bin/bash
module="fourmb_device_driver"
device="fourmb_device_driver"
mode="664"
major=61

make clean
make
insmod ./$module.ko
rm -rf /dev/${device} #remove stale nodes
mknod /dev/${device} c $major 0
ln -sf ${device} /dev/${device}
chmod $mode /dev/${device}
