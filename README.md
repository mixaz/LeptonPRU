# LeptonPRU

Linux kernel device driver for FLIR Lepton 3 camera with SPI bit-banging on PRU 
core.

AM3358 TI SoC on Beaglebone Black (and its family) has 2 PRU cores, this driver uses 1 PRU core  to perform SPI bit-banging and Lepton VoSPI protocol handling, to not waste main ARM CPU resources.

## Building from source code

### Prerequisites

Install [BeagleLogic (BL) debian image](https://beaglelogic.readthedocs.io/en/latest/beaglelogic_system_image.html) with linux kernel 4.9. We will build on BBB, without cross-compiling.

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

Modify /boot/uEnv.txt to load leptonpru-00A0.dtbo on bootup, add following line to /boot/uEnv.txt:
```
uboot_overlay_pru=/lib/firmware/leptonpru-00A0.dtbo
```
It shall replace BL cape. If using other BBB image, you may need to enable uboot overlays as well:
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

Configure the pins, run in LeptonPRU/kernel:
```
./enable-leptonpru-pins.sh
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

Compile LeptunPruLib - userspace helper library, in LeptonPRU/library folder:
```
make
```

Compile and run console test app, in LeptonPRU/test folder:
```
make
./leptonpru-test
```
It prints 4 frames from the module to stdout. You can run QT test app to see the video stream in GUI, see below.

See the driver stats:
```
$ cat /sys/devices/virtual/misc/leptonpru/state
state: 1, queue:2, frames received: 6, dropped: 0, segments mismatch: 105, 
packets mismatch: 6000, resync: 1, discards found: 30, discard sync fails: 0
```

### Building QT example

To build the example you'll need qt4 SDK:
```
sudo apt-get install qt4-dev-tools
```

Compile LeptonPruLib library, as described above.

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

## Code sample

```
	int fd;
	LeptonPruContext ctx;
	
	// open LeptonPRU device driver
        fd = open("/dev/leptonpru", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open");
		assert(0);
	}

	// initialize frame buffers
        if(LeptonPru_init(&ctx,fd) < 0) {
		perror("LeptonPru_init");
		assert(0);
	}

	while(1) {
		
		// read next frame
		if(LeptonPru_next_frame(&ctx) < 0) {
			perror("LeptonPru_next_frame");
			break;
		}
		// minimal&maximum values in the image, just in case
		printf("frame:%d min: %d, max: %d",
			nn,ctx.curr_frame->min_val,ctx.curr_frame->max_val);

		<do whataever you need with image data at ctx.curr_frame->image>
		
	}

	// release frame buffers
        if(LeptonPru_release(&ctx) < 0) {
		perror("LeptonPru_release");
	}

	// release the driver
	close(fd);
```


## Start LeptonPRU on bootup

To load **leptonpru** kernel module on bootup you need to copy it in Linux drivers folder for example `/lib/modules/$(shell uname -r)/kernel/drivers/spi`. Or run
```
sudo make deploy_module
```
and add the module to `/etc/modules`, ie:
```
debian@beaglebone:~$ cat /etc/modules
# /etc/modules: kernel modules to load at boot time.
#
# This file contains the names of kernel modules that should be loaded
# at boot time, one per line. Lines beginning with "#" are ignored.

leptonpru
```

to update modules DB:
```
sudo depmod -a
```

To configure LeptonPRU pins at bootup time you can create a service, it will run `enable-leptonpru-pins.sh` on bootup time:
```
sudo make deploy_service
```
and then enable it:
```
sudo systemctl daemon-reload
sudo systemctl enable  enable-leptonpru-pins.service
```


## Cross compilation in dockross docker container

### Cross compiling PRU firmware

Dockcross provides docker images for ARM cross compilation https://github.com/dockcross/dockcross. There's `LeptonPRU/docker` folder to build an image with TI PRU tools preinstalled:
```
cd LeptonPRU/docker
./build_leptonpru.sh
docker run leptonpru > ../leptonpru
chmod +x ../leptonpru

cd ..
./leptonpru bash
cd firmware
make
```

It will take some time to download dockross docker images and PRU tools.

Exit dockcross and copy firmware/release/lepton-pru0.out and firmware/release/lepton-pru1.out to /lib/firmware/lepton-pru0-fw and /lib/firmware/lepton-pru1-fw on target Beaglebone.

Remark: `LeptonPRU/docker/Dockerfimage` uses dockcross armv7 image modified for ARMv7-A, not in mainline at the time of writing. PR for that submitted: https://github.com/dockcross/dockcross/pull/307


### Cross compiling kernel

To cross compile the module you'll need kernel sources on host PC, for kernel revision used on target device (BBB). Though we do not need full kernel recompilation, we do need some files generated during kernel building.

To build kernel we also need its configuration file taken from target device, located at /boot/config-$(uname -r).

In LeptonPRU/kernel folder there's `cross-compile-kernel.sh` script which downloads kernel sources for given kernel revision, applies provided configuration file from target device, then builds the kernel. It can take not small time (but still shorter than building on BBB itself), feel you free to modify cross-compile-kernel.sh to speed up the process (ie using more cores instead `-j3` option, or enabling CCACHE).

I assume you already have `LeptonPRU/leptonpru` script which starts dockcross docker container, if not - refer `cross compiling PRU firmware` section above. Since we do not need PRU tools to compile the kernel, original dockross container can be used as well.
```
cd LeptonPRU
./leptonpru bash
cd kernel
./cross-compile-kernel.sh 4.9.36-ti-r45 ./config-4.9.36-ti-r45
```
4.9.36-ti-r45 is kernel version on my BBB, it's shown with `uname -r` command in ssh terminal.

./config-4.9.36-ti-r45 is kernel configuration file located at /boot/config-4.9.36-ti-r45 on my BBB.

### Cross compiling kernel module

Now we can cross compile the kernel module (also in leptonpru/dockcross session):
```
./cross-compile-module.sh 
```
leptonpru.ko file shall appear in current folder.

### Cross compiling device tree file

If you need to build dtbo file as well, you need to cross compule DTC (Device Tree Compiler) first (also in leptonpru/dockcross session):
```
./cross-compile-dtc.sh
```
Now you can compile dts file to dtbo binary (also in leptonpru/dockcross session):
```
./cross-compile-overlay.sh
```

## Some Additional info and links

There're some Lepton tech specs in docs folder.

Source code of this driver is based on BeagleLogic. BL uses 2 PRU, we use only one. BL needs exact timings, while Lepton camera can deal with flixible SPI rates 2-20Mhz. Currently FW for the second PRU is also loaded but it does nothing, shall be removed.

Frames are sent to userspace app via a cyclic queue, depth of 4. If the app can't process the frames fast enough (buffer overrun case), then the last frame is being overwritten by more recent frames.

The frames are sent via 4 mmap buffers, to avoid extra kernel-to-userspace transfers. PRU writes to that RAM directly, and it is shared with userspace app.

When you read from device file /dev/leptonpru, index of the first frame in the queue is returned. Userspace app uses the index to get frame data from appropriate mmap buffer. If there's no new frames (the queue is empty) then read operation blocks, alternativelly you can set non-block mode for the file descriptor to get a read() call return immediately with zero bytes being read.

/sys/devices/virtual/misc/leptonpru/ provides FS class attributes to handle driver. Useful ones are state and buffers, state prints statistics, and can accept 0/1 as input to stop/start frames capturing.

When the app processes a frame, it releases the buffer by sending 1 to the device driver.

I2C pins are not used, the camera starts streaming to SPI by default, the driver works OK without any i2c commands. To control the module via i2c use FLIR SDK.

2 first uint16 words of frame buffer consist of minimum/maximum values in the frame, see QT example.

Lepton packet CRC is not checked.

Telemetry is not supported.

### Links

Some useful links just in case:

* [BEAGLEBONE PRU CODE IN C](http://catch22.eu/beaglebone/beaglebone-pru-c/) - highly recommended, please read! and check there other articles as well, how to use prussdrv
* [TI PRU Training, LAB 5](http://processors.wiki.ti.com/index.php/PRU_Training:_Hands-on_Labs#LAB_5:_RPMsg_Communication_between_ARM_and_PRU)
* [BeagleLogic: Building a logic analyzer with the PRUs](http://theembeddedkitchen.net/beaglelogic-building-a-logic-analyzer-with-the-prus-part-1/449)
* [BeagleLogic at github](https://github.com/abhishek-kakkar/BeagleLogic)
* [Ti AM33XX PRUSSv2 on eLinux.org](https://elinux.org/Ti_AM33XX_PRUSSv2)
* [PRU Cookbook](https://markayoder.github.io/PRUCookbook/)
* [Beaglebone PRU shared memory in C](http://catch22.eu/beaglebone/beaglebone-pru-ipc/)
* [Beaglebone PRU DMA support](https://github.com/maciejjo/beaglebone-pru-dma)
* [Using Device Trees To Configure PRU IO Pins](http://www.ofitselfso.com/BeagleNotes/UsingDeviceTreesToConfigurePRUIOPins.php)
* [Simple MMAP implementation for Linux driver](https://stackoverflow.com/a/45645732/1028256)

