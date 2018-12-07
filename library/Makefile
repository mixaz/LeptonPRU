# Makefile for LeptonPRU userland library
#
# Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#

all: libleptonpru.a

LeptonPruLib.o: LeptonPruLib.c ../include/LeptonPruLib.h
	gcc -c LeptonPruLib.c -I../include

libleptonpru.a: LeptonPruLib.o
	ar rcs libleptonpru.a LeptonPruLib.o

clean:
	rm *.o *.a

