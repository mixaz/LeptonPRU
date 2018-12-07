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

// Dump raw packets data
//#define RAW_DATA

/*
 * Lepton camera frame attributes 
 */
 
// segments location in frame:
// 0,1
// 2,3
#define SEGMENT_WIDTH			(IMAGE_WIDTH/2)
#define SEGMENT_HEIGHT			(IMAGE_HEIGHT/2)

#define PACKET_SIZE_UINT16 		(SEGMENT_WIDTH+2)   
// VoSPI packet size in bytes
#define PACKET_SIZE 				(PACKET_SIZE_UINT16*2) 

#define NUMBER_OF_SEGMENTS 	4

#define PACKETS_PER_SEGMENT 	SEGMENT_HEIGHT
#define PACKETS_PER_FRAME 	(PACKETS_PER_SEGMENT*NUMBER_OF_SEGMENTS)
#define FRAME_SIZE_UINT16 		(PACKET_SIZE_UINT16*PACKETS_PER_FRAME)
#define FRAME_SIZE 				(PACKET_SIZE*PACKETS_PER_FRAME)

#define SEGMENT_SIZE_UINT16 	(PACKETS_PER_SEGMENT*PACKET_SIZE_UINT16)
#define SEGMENT_SIZE 			(SEGMENT_SIZE_UINT16*2)

/* ioctl calls that can be issued on /dev/leptonpru */

#define IOCTL_BL_GET_VERSION        		_IOR('k', 0x20, u32)
#define IOCTL_BL_GET_PACKET_SIZE        	_IOR('k', 0x21, u32)
#define IOCTL_BL_GET_IMAGE_WIDTH        	_IOR('k', 0x22, u32)
#define IOCTL_BL_GET_IMAGE_HEIGHT        	_IOR('k', 0x23, u32)
#define IOCTL_BL_GET_FRAME_SIZE        	_IOR('k', 0x24, u32)

#define IOCTL_BL_CACHE_INVALIDATE    _IO('k', 0x25)

#define IOCTL_BL_GET_BUFFER_SIZE    _IOR('k', 0x26, u32)

#define IOCTL_BL_GET_BUFUNIT_SIZE   _IOR('k', 0x27, u32)

#define IOCTL_BL_START               _IO('k', 0x29)
#define IOCTL_BL_STOP                _IO('k', 0x2A)

/* Lepton3 image size
 */
#define IMAGE_WIDTH		160
#define IMAGE_HEIGHT	120

/*
 * mmap buffer structure
 */
 #ifndef RAW_DATA
 typedef struct _leptonpru_mmap  {
	 uint32_t min_val;		 // min value in image
	 uint32_t max_val;		 // max value in image
	 uint16_t image[IMAGE_WIDTH * IMAGE_HEIGHT];
 } leptonpru_mmap;
 #else
 typedef struct _leptonpru_mmap  {
	 uint32_t min_val;		 
	 uint32_t max_val;		 
	 uint16_t image[FRAME_SIZE_UINT16];
 } leptonpru_mmap;
 #endif

// max frames number. Must be power of 2
#define FRAMES_NUMBER 			4

#endif /* LEPTONPRU_H_ */
