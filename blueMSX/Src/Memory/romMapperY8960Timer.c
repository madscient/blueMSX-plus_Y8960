/*****************************************************************************
**
** Y8960 cartridge - the MSX-TIMER block: four counters with an interrupt.
**
** Copyright (C) 2026 madscient
** See https://github.com/madscient/blueMSX-plus_Y8960 for change history.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/
#include "romMapperY8960.h"
#include "MediaDb.h"
#include "DeviceManager.h"
#include <stdlib.h>

typedef struct {
    int deviceHandle;
} RomMapperY8960Timer;

static void destroy(RomMapperY8960Timer* rm)
{
    deviceManagerUnregister(rm->deviceHandle);

    free(rm);
}

int romMapperY8960TimerCreate(void)
{
    DeviceCallbacks callbacks = { destroy, NULL, NULL, NULL };
    RomMapperY8960Timer* rm = (RomMapperY8960Timer*)calloc(1, sizeof(RomMapperY8960Timer));

    rm->deviceHandle = deviceManagerRegister(ROM_Y8960TIMER, &callbacks, rm);

    return 1;
}
