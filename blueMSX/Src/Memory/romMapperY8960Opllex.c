/*****************************************************************************
**
** Y8960 cartridge - the OPLLEX block: YM2413 with a per-channel preset bank.
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
#include "Y8960Opllex.h"
#include "AudioMixer.h"
#include <stdlib.h>

/* The first circuit answers at MSX-MUSIC's own addresses so that software
** written for a built-in FM sound chip drives it unchanged; the second sits
** below it. Which enabler bit opens which is the FM-PAC reading, not the
** current RTL's -- see romMapperY8960.h and the plan's section 3.3. */
#define Y8960_OPLL0_ADDRESS 0x7C
#define Y8960_OPLL0_DATA    0x7D
#define Y8960_OPLL1_ADDRESS 0x7A
#define Y8960_OPLL1_DATA    0x7B

typedef struct {
    int              deviceHandle;
    Y8960OpllexChip* chip[2];
} RomMapperY8960Opllex;

static void writeIo(RomMapperY8960Opllex* rm, UInt16 port, UInt8 value)
{
    int index = (port == Y8960_OPLL1_ADDRESS || port == Y8960_OPLL1_DATA) ? 1 : 0;

    if (!y8960IoEnabled(index ? Y8960_IO_OPLL1 : Y8960_IO_OPLL0)) {
        return;
    }

    if (port == Y8960_OPLL0_DATA || port == Y8960_OPLL1_DATA) {
        y8960OpllexWriteData(rm->chip[index], value);
    }
    else {
        y8960OpllexWriteAddress(rm->chip[index], value);
    }
}

/* The tunnel is not gated: the enablers live in the same window and could not
** be reached otherwise. */
static void tunnel0(void* ref, int port, UInt8 value)
{
    RomMapperY8960Opllex* rm = (RomMapperY8960Opllex*)ref;

    if (port) {
        y8960OpllexWriteData(rm->chip[0], value);
    }
    else {
        y8960OpllexWriteAddress(rm->chip[0], value);
    }
}

static void tunnel1(void* ref, int port, UInt8 value)
{
    RomMapperY8960Opllex* rm = (RomMapperY8960Opllex*)ref;

    if (port) {
        y8960OpllexWriteData(rm->chip[1], value);
    }
    else {
        y8960OpllexWriteAddress(rm->chip[1], value);
    }
}

static void reset(RomMapperY8960Opllex* rm)
{
    y8960OpllexReset(rm->chip[0]);
    y8960OpllexReset(rm->chip[1]);
}

static void saveState(RomMapperY8960Opllex* rm)
{
    y8960OpllexSaveState(rm->chip[0]);
    y8960OpllexSaveState(rm->chip[1]);
}

static void loadState(RomMapperY8960Opllex* rm)
{
    y8960OpllexLoadState(rm->chip[0]);
    y8960OpllexLoadState(rm->chip[1]);
}

static void destroy(RomMapperY8960Opllex* rm)
{
    y8960UnregisterTunnel(Y8960_TUN_OPLL0, rm);
    y8960UnregisterTunnel(Y8960_TUN_OPLL1, rm);

    ioPortUnregister(Y8960_OPLL0_ADDRESS, rm);
    ioPortUnregister(Y8960_OPLL0_DATA, rm);
    ioPortUnregister(Y8960_OPLL1_ADDRESS, rm);
    ioPortUnregister(Y8960_OPLL1_DATA, rm);

    y8960OpllexDestroy(rm->chip[0]);
    y8960OpllexDestroy(rm->chip[1]);

    deviceManagerUnregister(rm->deviceHandle);

    free(rm);
}

int romMapperY8960OpllexCreate(void)
{
    DeviceCallbacks callbacks = { destroy, reset, saveState, loadState };
    RomMapperY8960Opllex* rm = (RomMapperY8960Opllex*)calloc(1, sizeof(RomMapperY8960Opllex));

    rm->deviceHandle = deviceManagerRegister(ROM_Y8960OPLLEX, &callbacks, rm);

    rm->chip[0] = y8960OpllexCreate(boardGetMixer(), "Y8960 OPLLEX 0");
    rm->chip[1] = y8960OpllexCreate(boardGetMixer(), "Y8960 OPLLEX 1");

    /* Write only, as the FM-PAC's own 7Ch-7Dh are: these addresses are shared
    ** with the machine's MSX-MUSIC when one is present. */
    ioPortRegister(Y8960_OPLL0_ADDRESS, NULL, writeIo, rm);
    ioPortRegister(Y8960_OPLL0_DATA, NULL, writeIo, rm);
    ioPortRegister(Y8960_OPLL1_ADDRESS, NULL, writeIo, rm);
    ioPortRegister(Y8960_OPLL1_DATA, NULL, writeIo, rm);

    y8960RegisterTunnel(Y8960_TUN_OPLL0, tunnel0, rm);
    y8960RegisterTunnel(Y8960_TUN_OPLL1, tunnel1, rm);

    reset(rm);

    return 1;
}
