/*
 * PRU Firmware for IR camera FLIR Lepton 3.
 *
 * This file is a part of the LeptonPRU project.
 *
 * Copyright (C) 2018-2020 Mikhail Zemlyanukha <gmixaz@gmail.com>
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

#define STATE_STOP        0
#define STATE_RUN         1
#define STATE_WAIT_GPS    2

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
struct capture_context volatile cxt __attribute__((location(0))) = {0};

static uint8_t state_run;

static uint16_t gps_100hz_count;
static uint16_t gps_100hz_trigger;
static uint16_t pin_gps_100hz_mask;

static uint16_t samples_count;
static leptonpru_mmap *mmap_buf;
static uint32_t *buffer_ptr;

static void init_start(void) {
    cxt.frames_received = cxt.frames_dropped = 0;
    cxt.list_start = cxt.list_end = 0;
    samples_count = 0;
    gps_100hz_count = 0;
    gps_100hz_trigger = (uint16_t)(__R31 & pin_gps_100hz_mask);
    mmap_buf = (leptonpru_mmap *)(cxt.list_head[LIST_COUNTER_PSY(0)].dma_start_addr);
    buffer_ptr = mmap_buf->image;
    mmap_buf->start_time = cxt.start_time;
}

static void init_stop(void) {
    /* Clear all pending interrupts */
    CT_INTC.SECR0 = 0xFFFFFFFF;
    // allow some time for ARM to prepare for SYSEV_PRU0_TO_ARM_B handling
    // FIXME: this stops count_10msec counter, but it's not critical?
    __delay_cycles(10000000);
    /* Signal completion */
    SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_B);

    /* Reset PRU1 and our state */
    //  PCTRL_OTHER(0x0000) &= (uint16_t)~CONTROL_SOFT_RST_N;
}

static int handle_command(uint32_t cmd) {
    switch (cmd) {
        case CMD_STOP:
            init_stop();
            state_run = STATE_STOP;
            return 0;
        case CMD_START:
            state_run = STATE_WAIT_GPS;
            // fail through
        case CMD_CONFIGURE:
            if(cxt.sample_rate == 0) {
              cxt.sample_rate = DELAY_NS;
            }
            pin_gps_100hz_mask = (1 << cxt.pin_gps_100hz);
            init_pwm();
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
    state_run = STATE_STOP;

    handle_command(CMD_CONFIGURE);

    uint32_t packet;

    uint16_t gps_100hz_trigger_new;

    while (1) {

        wait_for_pwm_timer();
        packet  = (__R31 & 0xFFFF) | (gps_100hz_count << 16);

        cxt.start_time += cxt.sample_rate;

        gps_100hz_trigger_new = (uint16_t)(__R31 & pin_gps_100hz_mask);
//        cxt.debug = gps_100hz_trigger_new;
//        cxt.debug1 = pin_gps_100hz_mask;

        if (gps_100hz_trigger != gps_100hz_trigger_new) {
            gps_100hz_trigger = gps_100hz_trigger_new;
            gps_100hz_count++;
        }

        if (state_run == STATE_WAIT_GPS) {
            if ((cxt.flags&FLAG_START_ON_GPS_100HZ)==0 || gps_100hz_trigger_new) {
                init_start();
                state_run = STATE_RUN;
            }
            continue;
        }
        if (state_run == STATE_RUN) {
            if (samples_count >= BUFFER_SIZE) {
                // if queue is full, let's re-write the last frame, to not stop the stream
                if (LIST_IS_FULL(cxt.list_start, cxt.list_end)) {
                    LIST_COUNTER_DEC(cxt.list_end);
                    cxt.frames_dropped++;
                }
                LIST_COUNTER_INC(cxt.list_end);
                cxt.frames_received++;

                samples_count = 0;
                mmap_buf = (leptonpru_mmap *) (cxt.list_head[LIST_COUNTER_PSY(cxt.list_end)].dma_start_addr);
                buffer_ptr = mmap_buf->image;
                mmap_buf->start_time = cxt.start_time;

                /* Signal frame completion */
                SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_A);

                // stop when needed number of frames collected
                if (cxt.max_frames != 0 && cxt.frames_received - cxt.frames_dropped >= cxt.max_frames) {
                    init_stop();
                    state_run = STATE_STOP;
                    continue;
                }
            }
            *buffer_ptr++ = packet;
            samples_count++;
        }
    }
}

// Initializes the PWM timer, used to control output transitions.
// Every DELAY_NS nanoseconds, interrupt 15 will fire
inline void init_pwm() {
    *PRU_INTC_GER = 1; // Enable global interrupts
    *ECAP_APRD = cxt.sample_rate / 5 - 1; // Set the period in cycles of 5 ns
    *ECAP_ECCTL2 = (1 << 9) /* APWM */ | (1 << 4) /* counting */;
    *ECAP_TSCTR = 0; // Clear counter
    *ECAP_ECEINT = 0x80; // Enable compare equal interrupt
    *ECAP_ECCLR = 0xff; // Clear interrupt flags
}

// Wait for the PWM timer to fire.
// see TRM 15.2.4.26
inline void wait_for_pwm_timer() {
    // Wait for timer compare interrupt
    while (!(__R31 & (1 << 30))) {
        /* Process received command */
        if (cxt.cmd != 0) {
            cxt.resp = handle_command(cxt.cmd);
            cxt.cmd = 0;
        }
        if (cxt.state_run != state_run) {
            cxt.state_run = state_run;
        }
    }
    *PRU_INTC_SICR = 15; // Clear interrupt
    *ECAP_ECCLR = 0xff;     // Clear interrupt flags
}
