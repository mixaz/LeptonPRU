/*
 * Userland app to stream data from a file to pins using PRU
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include <sys/types.h>
#include <sys/mman.h>

#include "LeptonPruLib.h"

int main(int argc, char **argv) {
    int fd, file_in, err;
    LeptonPruContext ctx;

    if (argc < 2) {
        perror("stream2pru <path_to_file>");
        assert(0);
    }

    file_in = open(argv[1], O_RDONLY | O_SYNC /* | O_NONBLOCK*/);
    if (fd < 0) {
            printf("Can't open input file %s",argv[1]);
            assert(0);
    }

    fd = open("/dev/leptonpru", O_RDWR | O_SYNC /* | O_NONBLOCK*/);
    if (fd < 0) {
            perror("Can't open /dev/leptonpru");
            assert(0);
    }

    // leptonpru kernel module may need some time to start
    sleep(1);

    if(LeptonPru_init(&ctx,fd) < 0) {
            perror("LeptonPru_init");
            assert(0);
    }

    while(1) {
        err = LeptonPru_next_frame(&ctx);
        if(err <= 0) {
            perror("LeptonPru_next_frame");
            break;
        }
        err = read(file_in,ctx.curr_frame->image,sizeof(ctx.curr_frame->image));
        if(err <= 0) {
            perror("read from input failed");
            break;
        }
    }

    if(LeptonPru_release(&ctx) < 0) {
        perror("LeptonPru_release");
    }

    close(file_in);
    close(fd);

    return 0;
}
