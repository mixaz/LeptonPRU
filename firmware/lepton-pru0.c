/*
 * PRU Firmware for IR camera FLIR Lepton 3.
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
 
/* We are compiling for PRU0 */
#define PRU0

#include <stdint.h>
#include <string.h>
#include <pru_cfg.h>
#include "resource_table_0.h"

#include "../include/leptonpru_int.h"

/* from https://gist.github.com/shirriff/2a7cf2f1adb37011da827f1c7f47b992 */
#define DELAY_NS 500000 // Use 500000000 for 0.5 second delay
#define COUNT_10MS 20   // counter for 100Hz calculated from DELAY_NS

// PRU Interrupt control registers
#define PRU_INTC 0x00020000 // Start of PRU INTC registers TRM 4.3.1.2
#define PRU_INTC_GER ((volatile uint32_t *)(PRU_INTC + 0x10)) // Global Interrupt Enable, TRM 4.5.3.3
#define PRU_INTC_SICR ((volatile uint32_t *)(PRU_INTC + 0x24)) // Interrupt, TRM 4.5.3.6
#define PRU_INTC_GPIR ((volatile uint32_t *)(PRU_INTC + 0x80)) // Interrupt, TRM 4.5.3.11

// PRU ECAP control registers (i.e. PWM used as a timer)
#define ECAP 0x00030000 // ECAP0 offset, TRM 4.3.1.2
// Using APWM mode (TRM 15.3.2.1) to get timer (TRM 15.3.3.5.1)
#define ECAP_TSCTR ((volatile uint32_t *)(ECAP + 0x00)) // 32-bit counter register, TRM 15.3.4.1.1
#define ECAP_APRD ((volatile uint32_t *)(ECAP + 0x10)) // Period shadow, TRM 15.3.4.1.5, aka CAP3
#define ECAP_ECCTL2 ((volatile uint32_t *)(ECAP + 0x2a)) // Control 2, TRM 15.3.4.1.8
#define ECAP_ECEINT ((volatile uint16_t *)(ECAP + 0x2c)) // Enable interrupt, TRM 15.3.4.1.9
#define ECAP_ECCLR ((volatile uint16_t *)(ECAP + 0x30)) // Clear flags, TRM 15.3.4.1.11

// Forward definitions
static void init_pwm();
static void wait_for_pwm_timer();

/* PRU/ARM shared memory */
struct capture_context cxt __attribute__((location(0))) = {0};

static uint8_t state_run;

/* TODO: Placeholder to change configuration parameters */
static int configure_capture() {
	return 0;
}

static int handle_command(uint32_t cmd) {
	switch (cmd) {
		case CMD_START:
			state_run = 3;
			return 0;

		case CMD_STOP:
			state_run = 2;
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

    // stopped state
    state_run = 0;

    init_pwm();

    cxt.frames_received = cxt.frames_dropped = 0;

    uint16_t count_10msec = 0;
    uint8_t count_10msec_0 = 0;

    uint32_t packet;

    uint16_t frames_count;
    leptonpru_mmap *mmap_buf;
    uint32_t *buffer_ptr;

    while (1) {
        /* Process received command */
        if (cxt.cmd != 0)
        {
          cxt.resp = handle_command(cxt.cmd);
          cxt.cmd = 0;
        }

        wait_for_pwm_timer();
        packet  = (__R31 & 0xFFFF) | (count_10msec << 16);

        // 100Hz counter always work regardless state
        count_10msec_0++;
        if (count_10msec_0 >= COUNT_10MS) {
          count_10msec++;
          count_10msec_0 = 0;
        }

        if (state_run == 1) {
          if (frames_count >= BUFFER_SIZE) {
            // if queue is full, let's re-write the last frame, to not stop the stream
            if (LIST_IS_FULL(cxt.list_start, cxt.list_end)) {
              LIST_COUNTER_DEC(cxt.list_end);
              cxt.frames_dropped++;
            }
            LIST_COUNTER_INC(cxt.list_end);
            cxt.frames_received++;
            frames_count = 0;
            mmap_buf = (leptonpru_mmap *)(cxt.list_head[LIST_COUNTER_PSY(cxt.list_end)].dma_start_addr);
            buffer_ptr = mmap_buf->image;

            /* Signal completion */
            SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_A);
          }
          *buffer_ptr++ = packet;
          frames_count++;
        }
        else if (state_run == 3) {
          frames_count = 0;
          // cxt.list_start, cxt.list_end are supposed to be set to 0 on host side
          mmap_buf = (leptonpru_mmap *)(cxt.list_head[LIST_COUNTER_PSY(cxt.list_end)].dma_start_addr);
          buffer_ptr = mmap_buf->image;
//          cxt.debug = buffer_ptr;
          state_run = 1;
        }
        else if (state_run == 2) {
          state_run = 0;

          /* Clear all pending interrupts */
          CT_INTC.SECR0 = 0xFFFFFFFF;
          // allow some time for ARM to prepare for SYSEV_PRU0_TO_ARM_B handling
          // FIXME: this stops count_10msec counter, but it's not critical?
          __delay_cycles(10000000);
          /* Signal completion */
          SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_B);

          /* Reset PRU1 and our state */
//	  PCTRL_OTHER(0x0000) &= (uint16_t)~CONTROL_SOFT_RST_N;
        }
    }
}

// Initializes the PWM timer, used to control output transitions.
// Every DELAY_NS nanoseconds, interrupt 15 will fire
inline void init_pwm() {
  *PRU_INTC_GER = 1; // Enable global interrupts
  *ECAP_APRD = DELAY_NS / 5 - 1; // Set the period in cycles of 5 ns
  *ECAP_ECCTL2 = (1<<9) /* APWM */ | (1<<4) /* counting */;
  *ECAP_TSCTR = 0; // Clear counter
  *ECAP_ECEINT = 0x80; // Enable compare equal interrupt
  *ECAP_ECCLR = 0xff; // Clear interrupt flags
}

// Wait for the PWM timer to fire.
// see TRM 15.2.4.26
inline void wait_for_pwm_timer() {
  while (!(__R31 & (1 << 30))) {} // Wait for timer compare interrupt
  *PRU_INTC_SICR = 15; // Clear interrupt
  *ECAP_ECCLR = 0xff; // Clear interrupt flags
}
