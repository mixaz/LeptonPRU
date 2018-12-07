/*
 * LeptonPRU userland library
 *
 * Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef LEPTONPRU_LIB_H_
#define LEPTONPRU_LIB_H_

#include "leptonpru.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fd;
    int err;
    leptonpru_mmap *frame_buffers[FRAMES_NUMBER];
    leptonpru_mmap *curr_frame;
} LeptonPruContext;

int LeptonPru_init(LeptonPruContext *ctx, int fd);
int LeptonPru_release(LeptonPruContext *ctx);
int LeptonPru_next_frame(LeptonPruContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LEPTONPRU_LIB_H_ */
