# Makefile for kernel modules and device tree script
# This file is a part of the LeptonPRU project

# Pfefix for cross-compiler toolchain (crosstool-ng in dockcross image)
XCC ?= arm-linux-gnueabihf-
#$(CROSS_ROOT)/bin/$(CROSS_TRIPLE)-

# Path to kernel sources relative to current directory
KSRC ?= /lib/modules/$(shell uname -r)/build
#/work/linux-stable-rcn-ee

# Device Tree Compiler location
DTC ?= /usr/bin/dtc
# /work/dtc-bin/bin/dtc

# Path to kernel SPI drivers
DRIVERS_SPI ?= /lib/modules/$(shell uname -r)/kernel/drivers/spi

# Module targets (run from host)
obj-m := leptonpru_out.o
leptonpru_out-objs :=  leptonpru.o prussdrv.o

# LeptonPRU include files
EXTRA_CFLAGS = -I$(PWD)/../include -I$(PWD)/../am335x_pru_package/pru_sw/app_loader/include

# -L$(PWD)/../am335x_pru_package/pru_sw/app_loader/lib -lprussdrv.a

all:
	@make -C $(KSRC) M=$(PWD) ARCH=arm CROSS_COMPILE=$(XCC) modules

clean:
	@make -C $(KSRC) M=$(PWD) ARCH=arm CROSS_COMPILE=$(XCC) clean
	rm leptonpru-00A0.dtbo

# Compile and install device tree
overlay: leptonpru-00A0.dtbo

leptonpru-00A0.dtbo: leptonpru-00A0.dts
	$(DTC) -O dtb -o leptonpru-00A0.dtbo -b 0 -@ leptonpru-00A0.dts

deploy_overlay:
	cp -v leptonpru-00A0.dtbo /lib/firmware

deploy_module:
	cp -v leptonpru.ko $(DRIVERS_SPI)

deploy_service:
	cp -v enable-leptonpru-pins.sh /usr/bin/
	cp -v enable-leptonpru-pins.service /lib/systemd/system

deploy_all: deploy_overlay deploy_module deploy_service

