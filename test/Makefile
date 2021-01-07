# Makefile for LeptonPRU tests
#
# Copyright (C) 2018-2020 Mikhail Zemlyanukha <gmixaz@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#

all: stream2pru

stream2pru: stream2pru.c ../library/libleptonpru.a
	$(CC) -o stream2pru stream2pru.c ../library/libleptonpru.a -I../include

clean:
	rm *.o stream2pru
