#!/bin/bash
# Configure LeptonPRU pins

# from /opt/scripts/boot/am335x_evm.sh

board=$(cat /proc/device-tree/model | sed "s/ /_/g" | tr -d '\000')
case "${board}" in
TI_AM335x_BeagleBone)
	;;
TI_AM335x_BeagleBone_Black)
        # Beaglebone Black pinout
        config-pin P8.12 pruout
        config-pin P9.27 pruin
        config-pin P9.30 pruout
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
        config-pin P2_30 pruout
        config-pin P2_28 pruin
        config-pin P2_32 pruout
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




