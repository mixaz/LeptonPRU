# Debian image for PRU Sampler

This README describes how to prepare custom Debian image for Beaglebone Black with preinstalled PRU Sampler firmware, main CPU software and boot scripts.

## Architecture

The system needs to grab data from BBB pins and log events when signal is changed on the pins. The events need to be logged with time stamps. Since BBB isn't connected to network, it can't use NTP servers to correct system clock to world time. The clock is set from GPS receiver, using `gpsd` and `chrony` services.

To grab pins signals PRU coprocessor is used, to take signals at once, and sampling can't be distorted by main CPU Linux task scheduler. PRU0-7 pins are used for data sampling, and PRU0 is 1PPS signal, timestamps are corrected to it.

PRU firmware samples data to buffers in RAM, which are shared to userland app by `leptonpru` kernel module. The data in buffers are raw, sampled with frequency of 10ms. The userland app watches for changes in the data, and logs them to file. It would be probably better to track changes on PRU side, so buffers won't be neceassary, and regular `rproc` API could be used instead to communicate the changes directly to userland app, without kernel module. Approach with kernel module and raw data was used for historical reason, it was specifiled in initial requirements.

## Boot sequence

BBB runs Debian image customized start all above mentioned units on boot. The boot sequence includes following steps:

1. `enable-leptonpru-pins.service` is started. Its purpose is to configure input pins used by GPS and for data being sampled on PRU0-7

2. `gpsd.service` is started, it connects to GPS receiver on UART and 1PPS pins

3. `chronyd` service is started, it connects to `gpsd` via socket.

4. `chrony-wait.service` service is started, its purpose is to wait till GPS gets positioned and `chronyd` synchronizes system clock with actual time from GPS.

5. `start-leptonpru-scanner.service`, it starts userland app as service. The service loads `leptonpru` kernel module which waits for 1PPS signal, and starts sampling data to userland app. The apps creates log files per each pin.

Below we describe configuration of the units in details.

## Customizing Debian image

Approach is following - an official BBB Debian image from https://beagleboard.org/latest-images is flashed to SD card, booted on BBB, configuration files added/modified as necessary, BBB gets rebooted and tested that it works as expected, by applying signals to 1PPS and PRU0-7 pins and checking that gog files are created in `/home/debian` folder. System journal is reviewed to see that initd units works properly.

Then dump of SD card is created with `dd` command, and it can be flashed to other SD cards.

We need to use image with kernel 4.14-ti, leptonpru kernel module is built against this version of kernel, and may fail on other.

### Modifications for u-boot

`/boot/uEnv.txt` is used by u-boot, mostly for Device Tree configuration via u-boot overlays. Check for following changes in `/boot/uEnv.txt`:
```
enable_uboot_overlays=1
uboot_overlay_pru=/lib/firmware/leptonpru-00A0.dtbo
```
`leptonpru-00A0.dtbo` is built along `leptonpru` kernel module, see LeptonPRU `README-build.md`, copy it to `/lib/firmware/`.

### Service for GPIO modes setup

Copy `initd/enable-leptonpru-pins.service` to `/etc/systemd/system/multi-user.target.wants/` and `initd/enable-leptonpru-pins.sh` to `/home/debian`.

You may want to disable some overlays in `uEnv.txt` to free GPIOs for your usage.

Example of `enable-leptonpru-pins.sh` with pin configuration:
```
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
```

```
$ sudo systemctl enable enable-leptonpru-pins
```

It enables `enable-leptonpru-pins` service on bootup.

### Installing GPSD service

```
$ sudo apt-get install gpsd
```

Edit `/etc/default/gpsd` for your GPS receiver. My GPS is connected to `/dev/ttO4`. See `enable-leptonpru-pins.sh` below where UART4 is enabled. See `initd/gpsd` as example.

```
$ sudo systemctl enable gpsd
```

It enables `gpsd` service on bootup.

### Installing Chrony service

```
$ sudo apt-get install chrony
```

Edit `/etc/chrony/chrony.conf` for chrony settings, I use shared memory to connect with `gpsd` with NMEA protocol. See `initd/chrony.conf` as example.

```
$ sudo systemctl enable chrony
```

It enables `chrony` service on bootup.

### Service to wait GPS fix on bootup

Copy `initd/chrony-wait.service` to `/etc/systemd/system/multi-user.target.wants/`. This service waits till `chrony` gets a datetime fix from `gpsd`, so the sampler application starts with correct system time.

```
$ sudo systemctl enable chrony-wait
```

It enables `chrony-wait` service on bootup.

### Service with sampler app

Copy `initd/start-leptonpru-scanner.service` to `/etc/systemd/system/multi-user.target.wants/` and `initd/start-leptonpru-scanner.sh` to `/home/debian`.

Also you need to copy `sampler` binary file to `/home/debian`. The `sampler` is built in `test` folder, see `README-build.md`.

The same you need `leptonpru.ko` kernel module in `/home/debian`. PRU firmware files needs to be placed to `/lib/firmware/lepton-pru0-fw`  `/lib/firmware/lepton-pru1-fw`. See `README-build.md` how to build that binaries.


```
$ sudo systemctl enable start-leptonpru-scanner
```

It enables `start-leptonpru-scanner` service on bootup.

## Testing

Reboot BBB and check status of services:

```
$ systemctl status gpsd chrony enable-leptonpru-pins chrony-wait.service start-leptonpru-scanner
```

You may need to check logs of the services, to make sure they do not have errors, for example:

```
$ journalctl -u gpsd
```

For troubleshoting `gpsd` and `chrony` check manuals of those tools.


## Make image from SD card

Power off BBB, insert the SD card into reader on PC, and dump it:

```
sudo dd if=/dev/sdb bs=512 count=7372800 of=./sd.img
```

This command dumps 4Gb (the size of root partition in BBB debian distro) to file `sd.img`, and can be flashed to any other SD card, using tools like Etcher https://www.balena.io/etcher/.


