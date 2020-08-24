#!/bin/bash
# Configure LeptonPRU pins

  # Beaglebone Black pinout
#        config-pin P8.12 pruout
#        config-pin P9.27 pruin
#        config-pin P9.30 pruout
  # NMEA GPS on UART4
  config-pin P9_11 uart
  config-pin P9_13 uart
  sleep 1
  stty -F /dev/ttyO4 speed 115200
