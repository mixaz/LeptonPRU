/*
 * Internal defines for kernel driver and PRU firmware.
 *
 * This file is a part of the LeptonPRU project.
 *
 * Copyright (C) 2018-2019 Mikhail Zemlyanukha <gmixaz@gmail.com>
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
#define CMD_START			1   /* start sampling */
#define CMD_STOP			2   /* stop sampling */
//#define CMD_CONFIGURE   	3   /* configure */

/* Define magic bytes for the structure */
#define FW_MAGIC	0x4C456060

/* PRU-side sample buffer descriptor */
struct buflist {
	uint32_t dma_start_addr;
	uint32_t dma_end_addr;
};

// control flags
#define FLAG_START_ON_GPS_100HZ         0x00000001

/* Shared structure containing PRU attributes */
struct capture_context {
    /* Magic bytes */
    uint32_t magic;         // Magic bytes, should be FW_MAGIC

    uint32_t cmd;           // Command from Linux host to us
    int32_t resp;           // Response code

    uint32_t frames_dropped;    // frame buffer under run (empty) cases
    uint32_t frames_received;
    uint32_t unexpected_cs;     // unexpected high CS

    uint32_t state_run;

    uint32_t debug;

    uint32_t list_start, list_end;    // start end end of frames queue in list_head

    struct buflist list_head[FRAMES_NUMBER];    // frames cycle queue
};

enum beaglelogic_states {
	STATE_BL_DISABLED,	/* Powered off (at module start) */
	STATE_BL_INITIALIZED,	/* Powered on */
	STATE_BL_MEMALLOCD,	/* Buffers allocated */
	STATE_BL_ARMED,		/* All Buffers DMA-mapped and configuration done */
	STATE_BL_RUNNING,	/* Data being captured */
	STATE_BL_REQUEST_STOP,	/* Stop requested */
	STATE_BL_ERROR  	/* Buffer overrun */
};

#endif /* LEPTONPRU_INT_H_ */
