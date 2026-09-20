/*****************************************************************************
**
** Y8960 cartridge - the DCSG block: two DCSG-equivalent circuits.
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
#include "SaveState.h"
#include "Board.h"
#include "IoPort.h"
#include "Y8960Dcsg.h"
#include "AudioMixer.h"
#include <stdlib.h>

/* Two circuits, one I/O port each. The chip latches its register from the
** byte written, so there is no address port to go with it.
**
** The direct ports put the first circuit at the lower address, 3Eh, while the
** window puts it at the higher one, 7FF1h. The two orderings were settled
** separately and neither follows from the other. */
#define Y8960_DCSG_PORT0    0x3E
#define Y8960_DCSG_PORT1    0x3F

typedef struct {
    int      deviceHandle;
    Y8960DcsgChip* chip[2];
} RomMapperY8960Dcsg;

static RomMapperY8960Dcsg* theDcsg = NULL;

static void writeIo(RomMapperY8960Dcsg* rm, UInt16 port, UInt8 value)
{
    int index = (port == Y8960_DCSG_PORT1) ? 1 : 0;

    if (!y8960IoEnabled(index ? Y8960_IO_DCSG1 : Y8960_IO_DCSG0)) {
        return;
    }

    y8960DcsgWriteData(rm->chip[index], port, value);
}

/* The tunnel is not gated: the enablers live in the same window and could not
** be reached otherwise. */
static void tunnel0(void* ref, int p, UInt8 value)
{
    RomMapperY8960Dcsg* rm = (RomMapperY8960Dcsg*)ref;

    y8960DcsgWriteData(rm->chip[0], Y8960_DCSG_PORT0, value);
}

static void tunnel1(void* ref, int p, UInt8 value)
{
    RomMapperY8960Dcsg* rm = (RomMapperY8960Dcsg*)ref;

    y8960DcsgWriteData(rm->chip[1], Y8960_DCSG_PORT1, value);
}

static void reset(RomMapperY8960Dcsg* rm)
{
    y8960DcsgReset(rm->chip[0]);
    y8960DcsgReset(rm->chip[1]);
}

static void saveState(RomMapperY8960Dcsg* rm)
{
    y8960DcsgSaveState(rm->chip[0]);
    y8960DcsgSaveState(rm->chip[1]);
}

static void loadState(RomMapperY8960Dcsg* rm)
{
    y8960DcsgLoadState(rm->chip[0]);
    y8960DcsgLoadState(rm->chip[1]);
}

static void destroy(RomMapperY8960Dcsg* rm)
{
    y8960UnregisterTunnel(Y8960_TUN_DCSG0, rm);
    y8960UnregisterTunnel(Y8960_TUN_DCSG1, rm);

    ioPortUnregister(Y8960_DCSG_PORT0, rm);
    ioPortUnregister(Y8960_DCSG_PORT1, rm);

    y8960DcsgDestroy(rm->chip[0]);
    y8960DcsgDestroy(rm->chip[1]);

    deviceManagerUnregister(rm->deviceHandle);

    free(rm);

    theDcsg = NULL;
}

void romMapperY8960DcsgDestroy(void)
{
    if (theDcsg != NULL) {
        destroy(theDcsg);
    }
}

int romMapperY8960DcsgCreate(void)
{
    DeviceCallbacks callbacks = { destroy, reset, saveState, loadState };
    RomMapperY8960Dcsg* rm = (RomMapperY8960Dcsg*)calloc(1, sizeof(RomMapperY8960Dcsg));

    rm->deviceHandle = deviceManagerRegister(ROM_Y8960DCSG, &callbacks, rm);

    theDcsg = rm;

    rm->chip[0] = y8960DcsgCreate(boardGetMixer(), "Y8960 DCSG 0");
    rm->chip[1] = y8960DcsgCreate(boardGetMixer(), "Y8960 DCSG 1");

    /* Write only: the chip has nothing to read back, and these ports may be
    ** shared with something else in the machine. */
    ioPortRegister(Y8960_DCSG_PORT0, NULL, writeIo, rm);
    ioPortRegister(Y8960_DCSG_PORT1, NULL, writeIo, rm);

    y8960RegisterTunnel(Y8960_TUN_DCSG0, tunnel0, rm);
    y8960RegisterTunnel(Y8960_TUN_DCSG1, tunnel1, rm);

    reset(rm);

    return 1;
}
