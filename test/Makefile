# Makefile for LeptonPRU userland library
#
# Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#

all: leptonpru-test

leptonpru-test: leptonpru-test.c ../library/libleptonpru.a
	cc -o leptonpru-test leptonpru-test.c ../library/libleptonpru.a -I../include

clean:
	rm *.o leptonpru-test
