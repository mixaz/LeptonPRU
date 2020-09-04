/*
 * Userspace/Kernelspace common API.
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

#ifndef LEPTONPRU_H_
#define LEPTONPRU_H_

#define NANOSECONDS         1000000000L
#define MICROSECONDS        1000000
#define MILLISECONDS        1000

// Use 500000000 for 0.5 second delay
#define DELAY_NS 100000

#define BUFFER_SIZE         (160*120*2)

/*
 * mmap buffer structure
 */
typedef struct _leptonpru_mmap {
    uint64_t start_time;
    uint32_t sample_rate;
    uint32_t frame_number;
    uint8_t image[BUFFER_SIZE];
} leptonpru_mmap;

// max frames number. Must be power of 2
#define FRAMES_NUMBER 			4

#endif /* LEPTONPRU_H_ */
