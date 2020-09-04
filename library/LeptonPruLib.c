/*
 * Userspace helper library.
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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "leptonpru.h"
#include "LeptonPruLib.h"

/* 
 * Initializes context & allocates buffers 
 */
int LeptonPru_init(LeptonPruContext *ctx, int fd) {
    int i,off;
    int err = 0;
    leptonpru_mmap* mm;

    size_t psize = sysconf(_SC_PAGESIZE);
    int frame_size = sizeof(leptonpru_mmap);

    ctx->fd = fd;
    ctx->curr_frame = NULL;
	
    for(i=0; i<FRAMES_NUMBER; i++) {
        off = ((i*frame_size+psize-1)/psize) * psize;
        mm = (leptonpru_mmap*)mmap(NULL, frame_size, PROT_READ, MAP_SHARED, fd, off);
        if (mm == MAP_FAILED) {
            perror("LeptonPru_init: mmap");
            err = -1;
                break;
            }
        ctx->frame_buffers[i] = mm;
//    	printf("%d: %x\n", i, ctx->frame_buffers[i]);
    }
    if(err) {
        ctx->err = err;
        LeptonPru_release(ctx);
    }
    return err;
}

/*
 * Release frame buffers
 */
int LeptonPru_release(LeptonPruContext *ctx) {
    int i;
    int frame_size = sizeof(leptonpru_mmap);
    int err = 0;

    for(i=0; i<FRAMES_NUMBER; i++) {
	if (ctx->frame_buffers[i] != NULL) {
	    if ((err=munmap(ctx->frame_buffers[i], frame_size))) {
		perror("LeptonPru_release: munmap");
		ctx->err = err;
	    }
	    ctx->frame_buffers[i] = NULL;
        }
    }
    return err;
}

/*
 * Read next frame into ctx->curr_frame. 
 * Returns 0 (or blocks if file opened in blocking mode) if no buffer ready, 1 if new frame was read.
 * Otherwise negative error is returned.
 */
int LeptonPru_next_frame(LeptonPruContext *ctx) {
    uint8_t cc;

    // release previous frame, if any
    if(ctx->curr_frame) {
        ctx->curr_frame = NULL;
        cc = 1;
        ctx->err = write(ctx->fd,&cc,1);
//        printf("writing %d\n",cc);
        if(ctx->err < 0) {
	        return ctx->err;
        }
    }

    ctx->err = read(ctx->fd,&cc,1);
//    printf("reading %d\n",cc);

    if(ctx->err <= 0)
	    return ctx->err;

    ctx->curr_frame = ctx->frame_buffers[cc];
    ctx->cc = cc;
    return 1;
}

