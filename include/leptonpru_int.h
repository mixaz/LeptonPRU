/*
 * Internal defines for Lepton PRU
 *
 * Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef LEPTONPRU_INT_H_
#define LEPTONPRU_INT_H_

#include "leptonpru.h"

/*
 * Lepton camera frame attributes 
 */
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

/*
 * Old school cycle buffer magic 
 */
#define FRAMES_NUMBER_LGC 	(FRAMES_NUMBER*2)
// physical mask
#define FRAMES_MASK_PSY 		0x3
// logical mask
#define FRAMES_MASK_LGC 		0x7

#define LIST_IS_EMPTY(start,end)	((start) == (end))
//#define LIST_IS_FULL(start,end)	(((start) ^ (end)) & (FRAMES_MASK_PSY ^ FRAMES_MASK_LGC))
// distance between end-start > FRAMES_NUMBER
#define LIST_SIZE(start,end) 		(((end)+FRAMES_NUMBER_LGC-(start))&FRAMES_MASK_LGC)
#define LIST_IS_FULL(start,end)	(LIST_SIZE(start,end)>=4)
// inc logic counter
#define LIST_COUNTER_INC(counter) (counter) = ((counter) + 1) & FRAMES_MASK_LGC;
#define LIST_COUNTER_INC2(counter,inc) (counter) = ((counter) + (inc)) & FRAMES_MASK_LGC;
#define LIST_COUNTER_DEC(counter) (counter) = ((counter) + (FRAMES_NUMBER_LGC-1)) & FRAMES_MASK_LGC;

#define LIST_COUNTER_PSY(counter) ((counter)&FRAMES_MASK_PSY)

/* Commands */
#define CMD_GET_VERSION	1   /* Firmware version */
#define CMD_GET_MAX_SG	2   /* Get the max number of bufferlist entries */
#define CMD_SET_CONFIG 	3   /* Get the context pointer */
#define CMD_START			4   /* start sampling */
#define CMD_STOP			5   /* stop sampling */
#define CMD_TEST_FRAME	6   /* Generate a test frame */

/* Define magic bytes for the structure. "LEPT" ascii */
#define FW_MAGIC	0x4C455054

/* PRU-side sample buffer descriptor */
struct buflist {
	uint32_t dma_start_addr;
	uint32_t dma_end_addr;
};

/* Shared structure containing PRU attributes */
struct capture_context {
	/* Magic bytes */
	uint32_t magic;         // Magic bytes, should be 0x4C455054

	uint32_t cmd;           // Command from Linux host to us
	int32_t resp;            // Response code

	uint32_t dump_vospi;	// debug mode: dump VoSPI packets to mmap buffers
						// must be set before the buffers are allocated (start capturing)

	// counters
	uint32_t frames_dropped;		// count of dropped frames due to buffer overrun
	uint32_t segments_mismatch;	// count of dropped segments due to segment number mismatch
	uint32_t packets_mismatch;		// count of dropped packets due to packet number mismatch
	uint32_t resync_counter;		// out of sync counter, happens on packet mismatch, initially 1
	uint32_t frames_received;		// number of received frames
	uint32_t discards_found;
	uint32_t discard_sync_fails;

	uint32_t list_start,list_end;	// start end end of frames queue in list_head

	struct buflist list_head[FRAMES_NUMBER];	// frames cycle queue
};

enum beaglelogic_states {
	STATE_BL_DISABLED,	/* Powered off (at module start) */
	STATE_BL_INITIALIZED,	/* Powered on */
	STATE_BL_MEMALLOCD,	/* Buffers allocated */
	STATE_BL_ARMED,		/* All Buffers DMA-mapped and configuration done */
	STATE_BL_RUNNING,		/* Data being captured */
	STATE_BL_REQUEST_STOP,	/* Stop requested */
	STATE_BL_ERROR   		/* Buffer overrun */
};

#endif /* LEPTONPRU_INT_H_ */
