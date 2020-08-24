/*
 * PRU sampler.
 *
 * This file is a part of the LeptonPRU project.
 *
 * Copyright (C) 2020 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include <sys/types.h>
#include <sys/mman.h>

#include "LeptonPruLib.h"

#define PRU_PINS 16
#define PRU_PINS_MASK 0xFFFF

static uint16_t old_gpios = 0;

/*
 * Appends a record to file.
 */
static int write_event(int gpio_num, leptonpru_mmap *frame, int counter_100hz, int event) {
    FILE *fout = NULL;
    char file_name[20];
    struct timespec ts;

    uint64_t curr_time = frame->start_time + counter_100hz*NANOSECONDS/100;
    ts.tv_sec = curr_time / NANOSECONDS;
    ts.tv_nsec = curr_time % NANOSECONDS;

    struct tm tm = *localtime(&ts.tv_sec);

    int seconds_after_midnight = tm.tm_hour*60*60 + tm.tm_min*60 + tm.tm_sec;

    printf("%d: %05d %d\n", gpio_num, seconds_after_midnight, event);

    sprintf(file_name,"%04d-%03d_%02d", tm.tm_year + 1900, tm.tm_yday, gpio_num);
    fout = fopen(file_name, "a");
    fprintf(fout,"%05d %d\n", seconds_after_midnight, event);
    fclose(fout);
}

static void process_frame(leptonpru_mmap *frame, int edge) {
    int i,j;
    uint32_t bb, count, gpios;
    uint16_t mask, edge_mask;
    edge_mask = edge  ? 0xFFFF : 0;
    for(i=0; i<BUFFER_SIZE; i++) {
        bb = frame->image[i];
        count = bb >> PRU_PINS;
        gpios = bb & PRU_PINS_MASK;
        mask = 1;
        for(j=0; j<PRU_PINS; j++) {
            if((gpios&mask) ^ (old_gpios&mask)) {
                if((gpios&mask) ^ (edge_mask&mask)) {
                    write_event(j, frame, count, (gpios&mask) ? 1 : 0);
                }
            }
            mask <<= 1;
        }
        old_gpios = gpios;
    }
}

int main(int argc, char **argv) {
    int fd, err;
    LeptonPruContext ctx;

    int edge = 0;

    if (argc > 1) {
        sscanf(argv[1], "%d", &edge);
    }

    fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
    if (fd < 0) {
            perror("open");
            assert(0);
    }

    if(LeptonPru_init(&ctx,fd) < 0) {
            perror("LeptonPru_init");
            assert(0);
    }

    while(1) {

        err = LeptonPru_next_frame(&ctx);
        if(err < 0) {
            perror("LeptonPru_next_frame");
            break;
        }
        if(err == 0) {
            sleep(1);
            continue;
        }

        process_frame(ctx.curr_frame, edge);

    }

    if(LeptonPru_release(&ctx) < 0) {
        perror("LeptonPru_release");
    }

    close(fd);

    return 0;
}
