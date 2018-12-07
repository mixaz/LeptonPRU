/*
 * Userspace/Kernelspace common API for Lepton PRU
 *
 * Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef LEPTONPRU_H_
#define LEPTONPRU_H_

/* ioctl calls that can be issued on /dev/leptonpru */

#define IOCTL_GET_VERSION   			_IOR('k', 0x20, u32)
#define IOCTL_START               			_IO('k', 0x29)
#define IOCTL_STOP                			_IO('k', 0x2A)

/* Lepton3 image size
 */
#define IMAGE_WIDTH		160
#define IMAGE_HEIGHT	120

/*
 * mmap buffer structure
 */
 typedef struct _leptonpru_mmap  {
	 uint32_t min_val;		 // min value in image
	 uint32_t max_val;		 // max value in image
	 uint16_t image[IMAGE_WIDTH * IMAGE_HEIGHT];
 } leptonpru_mmap;

// max frames number. Must be power of 2
#define FRAMES_NUMBER 			4

#endif /* LEPTONPRU_H_ */
