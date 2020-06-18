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

#include <sys/types.h>
#include <sys/mman.h>

#include "LeptonPruLib.h"

static FILE *io,*iodir,*ioval;

static void open_gpios() {
    io = fopen("/sys/class/gpio/export", "w");
    fseek(io, 0, SEEK_SET);
    fprintf(io, "%d", 39);
    fflush(io);

    iodir = fopen("/sys/class/gpio/gpio39/direction", "w");
    fseek(iodir, 0, SEEK_SET);
    fprintf(iodir, "out");
    fflush(iodir);

    ioval = fopen("/sys/class/gpio/gpio39/value", "w");
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
    printf("----------------\n");
    for(i=0; i<BUFFER_SIZE; i++) {
        bb = buf[i];
        count = bb >> 16;
        gpios = bb & 0xFFFF;
        printf("%04x:%04x ",count,gpios);
    }
    printf("\n");
}

static void set_gpios(int val) {
    fprintf(ioval,"%d",val);
    fflush(ioval);
}

int main() {
    int fd, err;
    int gpios = 0;
    LeptonPruContext ctx;

    open_gpios();
    set_gpios(gpios);

    fd = open("/dev/leptonpru", O_RDWR | O_SYNC/* | O_NONBLOCK*/);
    if (fd < 0) {
            perror("open");
            assert(0);
    }

    if(LeptonPru_init(&ctx,fd) < 0) {
            perror("LeptonPru_init");
            assert(0);
    }

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
        print_frame1(ctx.curr_frame->image);

        if(nn++ >= 4)
                break;

        gpios ^= 1;
        set_gpios(gpios);

    }

    if(LeptonPru_release(&ctx) < 0) {
        perror("LeptonPru_release");
    }

    close(fd);

    close_gpios()

    return 0;
}

