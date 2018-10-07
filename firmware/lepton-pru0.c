/*
 * PRU Firmware for IR camera FLIR Lepton 3
 *
 * Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This file is a part of the LeptonPRU project
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
 
/* We are compiling for PRU0 */
#define PRU0

#include <stdint.h>
#include <string.h>
#include <pru_cfg.h>
#include "resource_table_0.h"

#include "../include/lepton.h"

//#define MOSI	15	//P8_11
#define CLK	14	//P8_12
#define MISO	5	//P9_27
#define CS	2	//P9_30

/*
 * Define firmware version
 */
#define MAJORVER	0
#define MINORVER	1

#define SET_PIN(bit,high) 		if(high) __R30 |= (1 << (bit)); else __R30 &= ~(1 << (bit));
#define INVERT_PIN(bit) 		__R30 ^= (1 << (bit));
#define CHECK_PIN(bit)		__R31 & (1 << (bit))

/* max bits to wait for discard packet header (3 packets for now) */
#define MAX_BITS_TO_DISCARD	(3*PACKETS_PER_FRAME*PACKET_SIZE_UINT16*16)
#define MAX_TRIES_TO_SYNC 		5
#define WRONG_SEGMENTS_TO_RESYNC 	8
#define WRONG_PACKETS_TO_RESYNC 	(PACKETS_PER_SEGMENT*100)

/* PRU/ARM shared memory */
struct capture_context cxt __attribute__((location(0))) = {0};

static uint8_t state_run = 0;
static uint8_t test_frame_run = 0;
static uint8_t test_frame_val = 0;

/*
 * read byte from SPI pins, SPI mode 3 - CPOL=1, CPHA=1, CS low on active
 */
static uint16_t spi_read16() 
{
	uint8_t i;
	uint16_t miso;

	miso = 0x0;
	
	for (i = 0; i < 16; i++) {
		miso <<= 1;
		SET_PIN(CLK,0);
		__delay_cycles(10);
		SET_PIN(CLK,1);
		__delay_cycles(2);
		
		if (__R31 & (1 << MISO))
			miso |= 0x01;
		else
			miso &= ~0x01;
	}
	return miso;
}

/* TODO: Placeholder to change configuration parameters */
static int configure_capture() {
	return 0;
}

/*
 * reset CS for 200ms to resync
 */
static void resync() {
	cxt.resync_counter++;
	// CS high to disable lepton
	SET_PIN(CS, 1);
	// CLK high for idle
	SET_PIN(CLK, 1);
	__delay_cycles(40000000L);
	// set CS low on active
	SET_PIN(CS,0);
	// wait a bit before we start CLK
	__delay_cycles(100L);
	
	SET_PIN(CLK, 0);
}

/* looks for 0x0fff 0xffff 0x0000 pattern */
/* returns number of read bits  */
static uint32_t wait_FFF_FFFF_0000(uint32_t maxBits) {
	uint32_t cnt1111,cnt0000;
	uint32_t bits = 0;
	
	cnt1111 = cnt0000 = 0;
	
	while((cnt1111 < 12+16 || cnt0000 < 16) && bits < maxBits) {
		
		SET_PIN(CLK,0);
		__delay_cycles(20);
		
		SET_PIN(CLK,1);
		if (__R31 & (1 << MISO)) {
			if(cnt0000) {
				cnt1111 = 0;
				cnt0000 = 0;
			}
 			cnt1111++;
		}
		else {
			cnt0000++;
		}
		__delay_cycles(20);
		bits++;
	}
	if(bits >= maxBits)
		cxt.discard_sync_fails++;
	else
		cxt.discards_found++;
	return bits;
}

static int sync_discard_packet() {
	uint32_t nn;
	int i;
	for(i=0; i<MAX_TRIES_TO_SYNC; i++) {
		if(wait_FFF_FFFF_0000(MAX_BITS_TO_DISCARD) >= MAX_BITS_TO_DISCARD)
			return -1;
		nn = wait_FFF_FFFF_0000(MAX_BITS_TO_DISCARD);
		if(nn >= MAX_BITS_TO_DISCARD)
			return -1;
		if(nn == PACKET_SIZE_UINT16*16) {
			// skip remaining bytes till end of the packet
			for(i=0; i<PACKET_SIZE_UINT16-3; i++) {
				spi_read16();
			}
			return 0;
		}
		cxt.discard_sync_fails++;
	}
	return -2;
}

/* shall return error when out of sync detected */
static int read_frame(int buf_idx)
{
        uint16_t i,j,k;
	int wrong_segment, wrong_packet;
	uint8_t segmentNumber;
	uint8_t packetNumber;

	uint16_t *packet_buf;
	uint16_t head0,head1,bb;		// head1 keeps CRC, ignored for now

	uint16_t frame_min = 0xFFFF;
	uint16_t frame_max = 0;
	uint16_t segment_min;
	uint16_t segment_max;
	uint16_t packet_min;
	uint16_t packet_max;

	uint16_t *dd = (uint16_t *)(cxt.list_head[buf_idx].dma_start_addr);

	wrong_segment = 0;
	for(i = 0; i < NUMBER_OF_SEGMENTS; ){
		wrong_packet = 0;
		segment_min = 0xFFFF;
		segment_max = 0;
		for(j=0;j<PACKETS_PER_SEGMENT;) {
			packet_buf = dd+(i*PACKETS_PER_SEGMENT+j)*PACKET_SIZE_UINT16;
			// TODO: move packet reading to the second PRU, for proper clocking
			// also would be good to have 2 packet buffers to read in parallel
			packet_min = 0xFFFF;
			packet_max = 0;
			packet_buf[0] = head0 = spi_read16();
			packet_buf[1] = head1 = spi_read16();
			for(k=2; k<PACKET_SIZE_UINT16; k++) {
				packet_buf[k] = bb = spi_read16();
				if(bb > packet_max)
					packet_max = bb;
				else if(bb < packet_min)
					packet_min = bb;
			}
			
			// skip discard packet (these are transmitted between segments)
			if((head0 & 0x0FFF) == 0x0FFF && head1 == 0xFFFF) {
				if(j != 0) {
					// unexpected discard -the segment was not fully transmitted
					cxt.packets_mismatch++;
				}
				j = 0;
				segment_min = 0xFFFF;
				segment_max = 0;
				continue;
			}
			
			packetNumber = head0 & 0xFF;
			// out of sync - wrong packet number received. 
			// a sync to discard packets shall be performed
			if(packetNumber != j) {
				cxt.packets_mismatch++;
				if(++wrong_packet >= WRONG_PACKETS_TO_RESYNC)
					return -2;
				j = 0;
				segment_min = 0xFFFF;
				segment_max = 0;
				continue;
			} 

			if(packetNumber == 20) {
				//reads the "ttt" number
				segmentNumber = head0 >> 12;
				//if it's not the segment expected reads again
				//for some reason segment are shifted, 1 down in result
				if(segmentNumber != i+1) {
					// wrong segment received, read next one till needed segment.
					// 2/3 frames are transmitted with segmentNumber = 0, shall be ignored.
					if(packetNumber != 0)
						cxt.segments_mismatch++;
					wrong_segment++;	// read segment till the end then dismiss it
				}
				else {
					wrong_segment = 0;
				}
			}
			
			// TODO: check packet CRC
			// next packet
			if(packet_max > segment_max)
				segment_max = packet_max;
			if(packet_min < segment_min)
				segment_min = packet_min;
			j++;
		}
		// next segment
		if(wrong_segment) {
			if(wrong_segment >= WRONG_SEGMENTS_TO_RESYNC)
				return -1;
			// dismiss the segment, read it anew
		}
		else {
			if(segment_max > frame_max)
				frame_max = segment_max;
			if(segment_min < frame_min)
				frame_min = segment_min;
			i++;
		}
	}	
	// storing min/max to header of the first frame
	// TODO: add API for that
	cxt.list_head[buf_idx].min_val  = dd[0] = frame_min;
	cxt.list_head[buf_idx].max_val = dd[1] = frame_max;
	cxt.frames_received++;
        return 0;	
}

static int handle_command(uint32_t cmd) {
	switch (cmd) {
		case CMD_GET_VERSION:
			return (MINORVER | (MAJORVER << 8));

		case CMD_SET_CONFIG:
			return configure_capture();

		case CMD_START:
			state_run = 3;
			return 0;

		case CMD_STOP:
			state_run = 2;
			return 0;

		case CMD_TEST_FRAME:
			if(LIST_IS_FULL(cxt.list_start,cxt.list_end)) {
				return -2;
			}
			test_frame_run = 1;
			return 0;

	}
	return -1;
}

void main()
{
	/* Enable OCP Master Port */
	CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
	cxt.magic = FW_MAGIC;

	/* Clear all interrupts */
	CT_INTC.SECR0 = 0xFFFFFFFF;

	while (1) {
		/* Process received command */
		if (cxt.cmd != 0)
		{
			cxt.resp = handle_command(cxt.cmd);
			cxt.cmd = 0;
		}

		/* generate test frame */
		if (test_frame_run == 1) {
			test_frame_run = 0;

			/* Clear all pending interrupts */
			CT_INTC.SECR0 = 0xFFFFFFFF;

			uint8_t *dd = (uint8_t *)cxt.list_head[LIST_COUNTER_PSY(cxt.list_end)].dma_start_addr;
			memset(dd,test_frame_val++,FRAME_SIZE);
			LIST_COUNTER_INC(cxt.list_end);
			
			/* Signal completion */
			SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_A);
		}
		
		if (state_run == 1) {

			// if queue is full, let's re-write the last frame, to not stop the stream
			if(LIST_IS_FULL(cxt.list_start,cxt.list_end)) {
				LIST_COUNTER_DEC(cxt.list_end);
				cxt.frames_dropped++;
			}
			if(read_frame(LIST_COUNTER_PSY(cxt.list_end))) {
				
				if(sync_discard_packet()) {
					resync();
					__delay_cycles(100000L);
					// TODO: implement hard reset here after few more attempts
				}
				
				continue;
			}
			LIST_COUNTER_INC(cxt.list_end)
				
			/* Signal frame completion */
			SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_A);

		}
		else if (state_run == 3) {
			state_run = 1;
			
			// reset counters
			cxt.list_start = cxt.list_end = 0;
			cxt.segments_mismatch = 0;
			cxt.packets_mismatch = 0;
			cxt.resync_counter = 0;
			cxt.frames_dropped = 0;
			cxt.frames_received = 0;
			cxt.discard_sync_fails = 0;
			cxt.discards_found = 0;
			
			/* Clear all pending interrupts */
			CT_INTC.SECR0 = 0xFFFFFFFF;
//			resume_other_pru();
			resync();
			sync_discard_packet();
		}
		else if (state_run == 2) {
			state_run = 0;
			// disable CS
			SET_PIN(CS,1);
			/* Clear all pending interrupts */
//			CT_INTC.SECR0 = 0xFFFFFFFF;
			/* Signal completion */
			SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_B);
			
			/* Reset PRU1 and our state */
//			PCTRL_OTHER(0x0000) &= (uint16_t)~CONTROL_SOFT_RST_N;
		}
	}
}


