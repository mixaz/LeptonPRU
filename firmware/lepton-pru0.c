/*
 * Streaming SPI Slave on PRU.
 * CPOL=0, CPHA=0, CS=0
 *
 * PRU outputs to SPI0 in GPIO mode (not in PRU direct mode), SPI0 pins are not available in this mode.
 * This is due to existing hardware PCB already using SPI0. For a 'right' solution proper pins (ie SPI1)
 * should be used instead, in PRU direct mode.
 *
 * PRU receives data from a cycle buffer (mapped via mmap to user space) of "frames". If the buffer overruns,
 * it's OK and userland app will be blocked in a write operation, till a free frame appears.
 * If the buffer under runs (gets empty) it means that the userland app can't fill in the buffer with proper speed,
 * it is counted as an error.
 *
 * If packet size (for low CS) is not multiple of 8 then it is also counted as an error.
 *
 * Copyright (C) 2020 Mikhail Zemlyanukha <gmixaz@gmail.com>
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

// http://vabi-robotics.blogspot.com/2013/10/register-access-to-gpios-of-beaglebone.html
#define OE_ADDR         0x134
#define GPIO_DATAOUT    0x13C
#define GPIO_DATAIN     0x138
#define GPIO0_ADDR      0x44E07000
#define GPIO1_ADDR      0x4804C000
#define GPIO2_ADDR      0x481AC000
#define GPIO3_ADDR      0x481AF000

// SPI CS pin (P9_17 on BBB)
#define GPIO0_5         5
// SPI SCLK pin (P9_22)
#define GPIO0_2         2
// SPI D1/MOSI pin (P9_18)
#define GPIO0_4         4

#define STATE_STOP              0
#define STATE_WAIT_CS_LOW       1
#define STATE_WAIT_CLK_HIGH     2
#define STATE_WAIT_CLK_LOW      3
#define STATE_WAIT_FRAME        4

#define GET_FRAME() { \
        leptonpru_mmap *mmap_buf = (leptonpru_mmap *) (cxt.list_head[LIST_COUNTER_PSY(cxt.list_start)].dma_start_addr); \
        buffer_ptr = mmap_buf->image; \
        samples_count = 0; \
    }

#define GET_BYTE() { \
        cur_byte = buffer_ptr[samples_count]; \
        cur_bit = 0; \
    }

/* PRU/ARM shared memory */
struct capture_context volatile cxt __attribute__((location(0))) = {0};

static uint8_t state_run;

static uint8_t cur_byte,cur_bit;
static uint32_t samples_count;
static uint8_t *buffer_ptr;

static void init_start(void) {
    cxt.frames_received = cxt.frames_dropped = cxt.unexpected_cs = 0;
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
            init_start();
            state_run = STATE_WAIT_FRAME;
            return 0;
    }
    return -1;
}

void main()
{
    uint8_t spi_cs, spi_clk;

    /* Enable OCP Master Port */
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
    cxt.magic = FW_MAGIC;

    /* Clear all interrupts */
    CT_INTC.SECR0 = 0xFFFFFFFF;

    // stopped state
    state_run = STATE_STOP;

    volatile uint32_t *gpio0 = (uint32_t *)GPIO0_ADDR;

    // set MISO pin for output and CS,CLK for input
    gpio0[OE_ADDR/4] |= (1 << GPIO0_5) | (1 << GPIO0_2);
    gpio0[OE_ADDR/4] &= 0xFFFFFFFF ^ (1 << GPIO0_4);

    while (1) {

        if(cxt.cmd != 0) {
            handle_command(cxt.cmd);
            cxt.cmd = 0;
        }
        if(cxt.state_run != state_run)
            cxt.state_run = state_run;

        // wait till we get input frame
        if (state_run == STATE_WAIT_FRAME) {
            if (!LIST_IS_EMPTY(cxt.list_start, cxt.list_end)) {
                GET_FRAME()
                GET_BYTE()
                state_run = STATE_WAIT_CS_LOW;
            }
            continue;
        }

        // get CS and CLK pins via GPIO
        spi_cs = gpio0[GPIO_DATAIN/4] & (1 << GPIO0_5);
        spi_clk = gpio0[GPIO_DATAIN/4] & (1 << GPIO0_2);

        if (state_run == STATE_WAIT_CS_LOW) {
            if (spi_cs == 0) {
                state_run = STATE_WAIT_CLK_LOW;
            }
            continue;
        }
        if (state_run == STATE_WAIT_CLK_LOW) {
            if (spi_cs) {
                cxt.unexpected_cs++;
                state_run = STATE_WAIT_CS_LOW;
                continue;
            }
            if (spi_clk == 0) {
                state_run = STATE_WAIT_CLK_HIGH;
            }
            continue;
        }
        if (state_run == STATE_WAIT_CLK_HIGH) {
            if (spi_cs) {
                state_run = STATE_WAIT_CS_LOW;
                continue;
            }
            if (spi_clk) {
                if (cur_byte & (1 << cur_bit)) {
                    gpio0[GPIO_DATAOUT/4] |= (1 << GPIO0_4);
                }
                else {
                    gpio0[GPIO_DATAOUT/4] &= 0xFFFFFFFF ^ (1 << GPIO0_4);
                }
                cur_bit++;
                if (cur_bit >= 8) {
                    samples_count++;
                    cxt.debug = samples_count;
                    if (samples_count >= BUFFER_SIZE) {
                        cxt.frames_received++;
                        LIST_COUNTER_INC(cxt.list_start);
                        if (LIST_IS_EMPTY(cxt.list_start, cxt.list_end)) {
                            // empty queue must not happen, counted as an error
                            cxt.frames_dropped++;
                            state_run = STATE_WAIT_FRAME;
                        }
                        GET_FRAME()

                        // Signal frame completion
                        SIGNAL_EVENT(SYSEV_PRU0_TO_ARM_A);
                    }
                    GET_BYTE()
                }
                state_run = STATE_WAIT_CLK_LOW;
            }
            continue;
        }

    }
}

