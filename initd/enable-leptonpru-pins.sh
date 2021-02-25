#!/bin/bash
# Configure LeptonPRU pins

  # Some pins for PRU sampling
  config-pin P9.28 pruin
  config-pin P9.29 pruin
  config-pin P9.30 pruin
  config-pin P9.31 pruin

  # NMEA GPS on UART4
  config-pin P9.11 uart
  config-pin P9.13 uart
  sleep 1
  stty -F /dev/ttyO4 speed 115200
