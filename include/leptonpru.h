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

#define BUFFER_SIZE         1024

/*
 * mmap buffer structure
 */
 typedef struct _leptonpru_mmap  {
	 uint32_t image[BUFFER_SIZE];
 } leptonpru_mmap;

// max frames number. Must be power of 2
#define FRAMES_NUMBER 			4

#endif /* LEPTONPRU_H_ */
