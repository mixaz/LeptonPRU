This debian image configured with following items:

1. leptonpru kernel module is loaded on boot (described in LeptonPRU README)
2. GPSD service starts on boot, configured in `/etc/defaults/gpsd` for NMEA receiver on UART4
3. CHRONY service starts on boot, configured in `/etc/chrony/chrony.conf` to connect to GPSD. Currently plain NMEA GPS source is used, without PPS (I do not have one with PPS to test). You may want to configure yours, see GPS and CHRONY manuals. Also this may be helpfull: https://gpsd.gitlab.io/gpsd/gpsd-time-service-howto.html
4. Bash script `/home/debian/enable-leptonpru-pins.sh` is started on boot, as a service. It sets UART mode for UART4 pins, 115200 baud, waits for system clock being set from GPS (by default the clock is year 2016, when year changes then it is considered as GPS fix received) and then `leptonpru-test`. When leptonpru-test starts it scans PRU0 pins and puts data to files in `/home/debian` folder, named after current date. To stop scanning run
```
sudo systemctl stop enable-leptonpru-pins
```
You may need to configure pins for `pruin` mode, do it in `enable-leptonpru-pins.sh`. Also you can
set sampling parameters via `sysfs` attributes in `/sys/devices/virtual/misc/leptonpru/` folder.

You may need to troubleshot GPSD and CHRONY. I use following commands:
```
cgps
chronyc sources
systemctl status gpsd chrony enable-leptonpru-pins
```

