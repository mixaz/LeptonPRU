/*
 * PRU1 Firmware for Lepton camera v3 driver
 *
 * Copyright (C) 2018-2019 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This file is a part of the LeptonPRU project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Apache License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdint.h>
#include "resource_table_1.h"

void main()
{
    // PRU1 is not used
    __halt();
}
