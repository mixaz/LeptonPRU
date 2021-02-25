#!/bin/bash

cd /home/debian
insmod ./leptonpru.ko
sleep 2
./sampler
