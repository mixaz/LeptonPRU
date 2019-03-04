# Makefile for kernel modules and device tree script
# This file is a part of the BeagleLogic project

# Path to kernel sources relative to current directory
KSRC ?= /lib/modules/$(shell uname -r)/build

# Path to kernel SPI drivers
DRIVERS_SPI ?= /lib/modules/$(shell uname -r)/kernel/drivers/spi

# Pins configuration file
BOARD ?= $(shell cat /proc/device-tree/model)
ifneq (,$(findstring PocketBeagle,$(BOARD)))
        PIN_CFG := enable-leptonpru_pb-pins.sh
else
        PIN_CFG := enable-leptonpru_bbb-pins.sh
endif

# Module targets (run from host)
obj-m := leptonpru.o

# LeptonPRU include files
EXTRA_CFLAGS=-I$(PWD)/../include

all:
	@make -C $(KSRC) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	@make -C $(KSRC) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean
	rm leptonpru-00A0.dtbo

# Compile and install device tree
overlay: leptonpru-00A0.dtbo

leptonpru-00A0.dtbo: leptonpru-00A0.dts
	dtc -O dtb -o leptonpru-00A0.dtbo -b 0 -@ leptonpru-00A0.dts

deploy_overlay:
	cp -v leptonpru-00A0.dtbo /lib/firmware

deploy_module:
	cp -v leptonpru.ko $(DRIVERS_SPI)

deploy_service:
	cp -v $(PIN_CFG) /usr/bin/enable-leptonpru-pins.sh
	cp -v enable-leptonpru-pins.service /lib/systemd/system

deploy_all: deploy_overlay deploy_module deploy_service

