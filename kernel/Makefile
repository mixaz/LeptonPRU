# Makefile for kernel modules and device tree script
# This file is a part of the BeagleLogic project

# Path to kernel sources relative to current directory
KSRC ?= /lib/modules/$(shell uname -r)/build

# Module targets (run from host)
obj-m := leptonpru.o
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
