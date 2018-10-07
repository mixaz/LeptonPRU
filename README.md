# LeptonPRU

Linux kernel device driver for FLIR Lepton 3 camera with SPI bit-banging on PRU 
core.

AM3358 TI SoC on Beaglebone Black (and its family) has 2 PRU cores, this driver uses 1 PRU to perform
SPI bit-banging and Lepton VoSPI protocol handling, to not waste main ARM CPU resources.

## Building from source code

### Prerequisites

Install [BeagleLogic debian image](https://beaglelogic.readthedocs.io/en/latest/beaglelogic_system_image.html) with linux kernel 4.9. We will build on BBB, without cross-compiling.

You may need to install build tools such as git and GCC, seems that they go with BL image by default.

```
sudo apt-get install build-essential
```

### Getting LeptonPRU source code

```
git clone git@github.com:mixaz/LeptonPRU.git
cd LeptonPRU
```

### Building PRU firmware

You need TI PRU SDK installed:
```
sudo apt-get install ti-pru-cgt-installer
```

Compile PRU firmware, run in LeptonPRU/firmware folder:
```
PRU_CGT=/usr/share/ti/cgt-pru make
```

copy release/lepton-pru0.out and release/lepton-pru1.out to /lib/firmware/lepton-pru0-fw and /lib/firmware/lepton-pru1-fw or just run
```
sudo su -c "PRU_CGT=/usr/share/ti/cgt-pru make install"
```

### Building kernel module

You need to install linux headers package:

```
sudo apt-get install linux-headers-$(uname -r)
```
The project was tested with 4.9 kernel. On 4.14 kernel it doesn't compile sinse some PRU APIs were changed.

Compile kernel module, run from LeptoPRU/kernel folder:

```
make
```

leptonpru.ko should appear in current folder.

Now compile leptonpru-00A0.dtbo:
```
make overlay
```

Copy leptonpru-00A0.dtbo to /lib/firmware/:
```
sudo make deploy_overlay
```

Modify uEnv.txt to load leptonpru-00A0.dtbo on bootup, add following like to /boot/uEnv.txt:
```
uboot_overlay_pru=/lib/firmware/leptonpru-00A0.dtbo
```
It shall replace BL cape. If using other BBB image, you may need to enable uboot oiverlays as well:
```
enable_uboot_overlays=1
```
It is enabled in BBB debian images by default AFAIK.

Now you can reboot:
```
sudo shutdown now
```

## Testing LeptonPRU driver

Connect the Lepton module to BBB, currently the pins are hardcoded to 
```
#define CLK	14	//P8_12
#define MISO	5	//P9_27
#define CS	2	//P9_30
```

Configure the pins, run in /LeptonPRU/kernel:
```
./pincfg.sh
```

Go to LeptonPRU/kernel folder and load the driver:
```
sudo insmod leptonpru.ko
```

You can see the leptonpru driver loaded:
```
$ lsmod
Module                  Size  Used by
leptonpru              12779  0
omap_aes_driver        24392  0
crypto_engine           7219  1 omap_aes_driver
pruss_soc_bus           4788  0
omap_sham              27230  0
omap_rng                5864  0
rng_core                9161  1 omap_rng
c_can_platform          7825  0
c_can                  12883  1 c_can_platform
```
And in dmesg logging:
```
[ 2632.653701] Lepton PRU loaded and initializing
[ 2632.654374] misc leptonpru: Valid PRU capture context structure found at offset 0000
[ 2632.654395] misc leptonpru: BeagleLogic PRU Firmware version: 0.1
[ 2632.654599] misc leptonpru: Successfully allocated 157440 bytes of memory.
[ 2632.654854] misc leptonpru: Lepton PRU initialized OK
```

 Compile and run console test app, in LeptonPRU/test folder:
```
cc leptonpru-test.c
./a.out
```
It prints 10 frames from the module to stdout. The data is in Lepton VoSPI format, it's just raw data in packets of 164 bytes length. You can run QT test app to see the video stream in GUI.

See the driver stats:
```
$ cat /sys/devices/virtual/misc/leptonpru/state
state: 1, queue:2, frames received: 13, dropped: 0, segments mismatch: 105, 
packets mismatch: 6000, resync: 1, discards found: 30, discard sync fails: 0
```

### Building QT example

To build the example you'll need qt4 SDK:
```
sudo apt-get install qt4-dev-tools
```

Compile QT example (modified version of an example from FLIR Lepton SDK, for RaspberryPI) in LeptonPRU/raspberrypi_video_Lepton3 folder:
```
qmake
make
```
You shall get raspberrypi_video binary in the current folder

You need X11 installed to run the app, follow these instructions: [Add X11 to a BeagleBone IoT image](https://gist.github.com/jadonk/39d0fcfc323347d88e995cdfee02bdad)

It is not necessary to have a monitor attached to HDMI port of BBB, you can run the example in SSH session:
```
ssh -Y <your BBB IP>
```
then run raspberrypi_video

See LeptonThread.cpp for code related to the driver usage.

## Some Additional info and links

There're some Lepton tech specs in docs folder.

Source code of this driver is based on BeagleLogic. BL uses 2 PRU, we use only one. BL needs exact timings, while Lepton camera can deal with flixible SPI rates 2-20Mhz. Currently FW for the second PRU is also loaded but it does nothing, shall be removed.

Frames are sent in VoSPI format - 4 segments for 160x120 image, see FLIR docs. See QT example
how to convert it to an image (LeptonThread.cpp).

Frames are sent to userland app via a cyclic queue, depth of 4. If the app can't process the frames fast enough (buffer overrun case), then the last frame is being overwritten by more recent frames.

The frames are sent via 4 mmap buffers, to avoid extra kernel-to-userspace transfers. PRU writes to that RAM
directly, and it is shared with userland app.

When you read from device file /dev/leptonpru, index of the first frame in the queue is returned. Userland app
uses the index to get frame data from appropriate mmap buffer. If there's no new frames (the queue is empty) then read operation blocks, alternativelly you can set non-block mode for the file descriptor to get a read() call return immediately with zero bytes being read.

/sys/devices/virtual/misc/leptonpru/ provides FS class attributes to handle driver. Useful ones are state and buffers, state prints statistics, and can accept 0/1 as input to stop/start frames capturing.

When the app processes a frame, it releases the buffer by sending 1 to the device driver.

I2C pins are not used, the camera starts streaming to SPI by default, the driver works OK without any i2c commands. To control the module via i2c use FLIR SDK.

2 first uint16 words of frame buffer consist of minimum/maximum values in the frame, see QT example.

Lepton packet CRC is not checked.

Telemetry is not supported.

### Links

* [FLIR Lepton Engineering Datasheet](docs/flir-lepton-engineering-datasheet.pdf)
* [FLIR Lepton 2 vs Lepton 3](docs/lepton-vs-lepton-3-app-note.pdf)
* [BeagleLogic at github](https://github.com/abhishek-kakkar/BeagleLogic)
* [Ti AM33XX PRUSSv2 on eLinux.org](https://elinux.org/Ti_AM33XX_PRUSSv2)
* [PRU Cookbook](https://markayoder.github.io/PRUCookbook/)
* [BeagleLogic: Building a logic analyzer with the PRUs](http://theembeddedkitchen.net/beaglelogic-building-a-logic-analyzer-with-the-prus-part-1/449)
* [Beaglebone PRU shared memory in C](http://catch22.eu/beaglebone/beaglebone-pru-ipc/)
* [SPI Master Controller on PRU](https://github.com/chanakya-vc/PRU-I2C_SPI_master/wiki/SPI-Master-Controller)
* [PRU SPI driver on the BeagleBone](https://github.com/giuliomoro/bb-pru-spi-duplex)
* [Beaglebone PRU DMA support](https://github.com/maciejjo/beaglebone-pru-dma)
* [QT4 cross-compiling for BBB](https://github.com/yongli-aus/qt-4.8.6-cross-compile-for-beaglebone-black)
* [Beagleboard:Expanding File System Partition On A microSD](https://elinux.org/Beagleboard:Expanding_File_System_Partition_On_A_microSD)
* [Using Device Trees To Configure PRU IO Pins](http://www.ofitselfso.com/BeagleNotes/UsingDeviceTreesToConfigurePRUIOPins.php)
* [Simple MMAP implementation for Linux driver](https://stackoverflow.com/a/45645732/1028256)
* [Add X11 to a BeagleBone IoT image](https://gist.github.com/jadonk/39d0fcfc323347d88e995cdfee02bdad)

