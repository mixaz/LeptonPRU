#!/bin/bash
# Configure pins

# from /opt/scripts/boot/am335x_evm.sh
board=$(cat /proc/device-tree/model | sed "s/ /_/g" | tr -d '\000')
case "${board}" in
TI_AM335x_BeagleBone)
	;;
TI_AM335x_BeagleBone_Black)
  # P9_19 wired to P9_22 (SPI CLK)
  config-pin P9_19 gpio
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
  # P1_28 wired to P1_8 (SPI CLK) (gpio13)
  config-pin P1_28 gpio
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

# CLK out
echo out > /sys/class/gpio/gpio13/direction

while [ True ]
do
    echo 1 > /sys/class/gpio/gpio13/value
#    cat      /sys/class/gpio/gpio2/value
    cat      /sys/class/gpio/gpio4/value
#    echo "---"
#    sleep 1
    echo 0 > /sys/class/gpio/gpio13/value
#    sleep 1
done

