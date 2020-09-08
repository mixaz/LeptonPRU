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

#define PRU_PINS 8
#define PRU_PINS_MASK 0xFF

static uint8_t old_gpios = 0;

/*
 * Appends a record to file.
 */
static int write_event(int gpio_num, leptonpru_mmap *frame, int packet_num, int frame_num, int event) {
    FILE *fout = NULL;
    char file_name[20];
    struct timespec ts;

    uint64_t curr_time = frame->start_time + packet_num*frame->sample_rate;
    ts.tv_sec = curr_time / NANOSECONDS;
    ts.tv_nsec = curr_time % NANOSECONDS;

    int msecs = (curr_time % NANOSECONDS) / 10000; //MICROSECONDS;

    struct tm tm = *localtime(&ts.tv_sec);

    int seconds_after_midnight = tm.tm_hour*60*60 + tm.tm_min*60 + tm.tm_sec;

    printf("%d: %05d.%05d %d\n", gpio_num, seconds_after_midnight, msecs, event);
//    printf("%d: %llu (%llu-%04d %d %05d) %05d.%05d %02d:%02d:%02d %d\n", gpio_num, curr_time, frame->start_time, packet_num, frame_num,
//            frame->frame_number, seconds_after_midnight, msecs, tm.tm_hour, tm.tm_min, tm.tm_sec, event);

    sprintf(file_name,"%04d-%03d_%02d", tm.tm_year + 1900, tm.tm_yday, gpio_num);
    fout = fopen(file_name, "a");
    fprintf(fout,"%05d.%05d %d\n", seconds_after_midnight, msecs, event);
    fclose(fout);
}

static uint64_t prev_start_time = 0L;

static void process_frame(int frame_num, leptonpru_mmap *frame) {
    int i,j;
    uint8_t gpios;
    uint8_t mask;
    if(frame->start_time <= prev_start_time) {
        printf("frame time mismatch: old %llu new %llu\n");
    }
    for(i=0; i<BUFFER_SIZE; i++) {
        gpios = frame->image[i];
        mask = 1;
        for(j=0; j<PRU_PINS; j++) {
            if((gpios&mask) ^ (old_gpios&mask)) {
                write_event(j, frame, i, frame_num, (gpios&mask) ? 1 : 0);
            }
            mask <<= 1;
        }
        old_gpios = gpios;
    }
}

int main(int argc, char **argv) {
    int fd, err;
    LeptonPruContext ctx;

    fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
    if (fd < 0) {
            perror("open");
            assert(0);
    }

    sleep(1);

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
        process_frame(ctx.cc,ctx.curr_frame);
    }

    if(LeptonPru_release(&ctx) < 0) {
        perror("LeptonPru_release");
    }

    close(fd);

    return 0;
}
