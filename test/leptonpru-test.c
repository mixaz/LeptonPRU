/*
 * Userspace console test.
 *
 * This file is a part of the LeptonPRU project.
 *
 * Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
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

static FILE *io,*iodir,*ioval;

static void open_gpios() {
    io = fopen("/sys/class/gpio/export", "w");
    fseek(io, 0, SEEK_SET);
    fprintf(io, "%d", 39);
    fflush(io);

    iodir = fopen("/sys/class/gpio/gpio66/direction", "w");
    fseek(iodir, 0, SEEK_SET);
    fprintf(iodir, "out");
    fflush(iodir);

    ioval = fopen("/sys/class/gpio/gpio66/value", "w");
    fseek(ioval, 0, SEEK_SET);
}

static void close_gpios() {
    fclose(io);
    fclose(iodir);
    fclose(ioval);
}

static void print_frame1(uint32_t *buf) {
    int i;
    uint32_t bb, count, gpios;
//    printf("----------------\n");
    for(i=0; i<10; i++) {
        bb = buf[i];
        count = bb >> 16;
        gpios = bb & 0xFFFF;
        printf("%04x:%04x ",count,gpios);
    }
    printf("...\n");
}

static void out_frame1(FILE *file, leptonpru_mmap *buf) {
    fwrite(buf->image,sizeof(buf->image),1,file);
}

static void set_gpios(int val) {
    fprintf(ioval,"%d",val);
    fflush(ioval);
}

int main(int argc, char **argv) {
    int fd, err;
    int gpios = 0;
    LeptonPruContext ctx;

    int frames_in_file = 10;

    if (argc > 1) {
        sscanf(argv[1], "%d", &frames_in_file);
    }

//    open_gpios();
//    set_gpios(gpios);

    fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
    if (fd < 0) {
            perror("open");
            assert(0);
    }

    if(LeptonPru_init(&ctx,fd) < 0) {
            perror("LeptonPru_init");
            assert(0);
    }

    FILE *fout = NULL;

    int nn = 0;

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
        if (fout == NULL) {
            char file_name[100];
            struct timespec ts;
            ts.tv_sec = ctx.curr_frame->start_time / NANOSECONDS;
            ts.tv_nsec = ctx.curr_frame->start_time % NANOSECONDS;
            struct tm tm = *localtime(&ts.tv_sec);

            sprintf(file_name,"%04d-%03d-%02d%02d%02d", tm.tm_year + 1900, tm.tm_yday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            printf("start_time: %llu\n",ctx.curr_frame->start_time);
            printf("Writing file: %s\n",file_name);
            fout = fopen(file_name, "wb");
        }

        print_frame1(ctx.curr_frame->image);
        out_frame1(fout,ctx.curr_frame);

        if (nn++ >= frames_in_file) {
            fclose(fout);
            fout = NULL;
            nn = 0;
        }

//        gpios ^= 1;
//        set_gpios(gpios);

    }

    if (fout != NULL) {
        fclose(fout);
        fout = NULL;
    }

    if(LeptonPru_release(&ctx) < 0) {
        perror("LeptonPru_release");
    }

    close(fd);

//    close_gpios()

    return 0;
}

