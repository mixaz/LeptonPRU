/*
 * Userspace/Kernelspace common API for Lepton PRU
 *
 * Copyright (C) 2018 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef LEPTONPRU_H_
#define LEPTONPRU_H_

enum beaglelogic_states {
	STATE_BL_DISABLED,	/* Powered off (at module start) */
	STATE_BL_INITIALIZED,	/* Powered on */
	STATE_BL_MEMALLOCD,	/* Buffers allocated */
	STATE_BL_ARMED,		/* All Buffers DMA-mapped and configuration done */
	STATE_BL_RUNNING,		/* Data being captured */
	STATE_BL_REQUEST_STOP,	/* Stop requested */
	STATE_BL_ERROR   		/* Buffer overrun */
};

/* ioctl calls that can be issued on /dev/leptonpru */

#define IOCTL_BL_GET_VERSION        _IOR('k', 0x20, u32)

#define IOCTL_BL_CACHE_INVALIDATE    _IO('k', 0x25)

#define IOCTL_BL_GET_BUFFER_SIZE    _IOR('k', 0x26, u32)

#define IOCTL_BL_GET_BUFUNIT_SIZE   _IOR('k', 0x27, u32)

#define IOCTL_BL_START               _IO('k', 0x29)
#define IOCTL_BL_STOP                _IO('k', 0x2A)

#endif /* LEPTONPRU_H_ */
