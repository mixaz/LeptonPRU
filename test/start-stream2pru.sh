#!/bin/bash

# from /opt/scripts/boot/am335x_evm.sh
board=$(cat /proc/device-tree/model | sed "s/ /_/g" | tr -d '\000')
case "${board}" in
TI_AM335x_BeagleBone)
	;;
TI_AM335x_BeagleBone_Black)
  # P9_19 wired to P9_22 (SPI CLK)
  config-pin P9_19 gpio
  # TBD
	;;
TI_AM335x_BeagleBone_Black_Wireless)
	;;
TI_AM335x_BeagleBone_Blue)
	;;
TI_AM335x_BeagleBone_Green)
	;;
TI_AM335x_BeagleBone_Green_Wireless)
	;;
TI_AM335x_BeagleLogic_Standalone)
	;;
TI_AM335x_P*)
  # PocketBeagle pinout
  # SPI CS (gpio5)
  config-pin P1_06 gpio
  # SPI CLK (gpio2)
  config-pin P1_08 gpio
  # SPI MISO (gpio3)
  config-pin P1_10 gpio
  # SPI MOSI (gpio4)
  config-pin P1_12 gpio
	;;
SanCloud_BeagleBone_Enhanced)
	;;
Octavo_Systems_OSD3358*)
	;;
TI_AM335x_BeagleBone_Black_RoboticsCape)
	;;
TI_AM335x_BeagleBone_Black_Wireless_RoboticsCape)
	;;
*)
	;;
esac

# CS in
echo in > /sys/class/gpio/gpio5/direction
# CLK in
echo in > /sys/class/gpio/gpio2/direction
# MOSI out
echo out > /sys/class/gpio/gpio4/direction
# MISO in
echo in > /sys/class/gpio/gpio3/direction

cd /home/debian
./stream2pru /dev/urandom
