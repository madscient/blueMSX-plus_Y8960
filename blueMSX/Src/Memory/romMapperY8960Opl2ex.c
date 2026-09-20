/*****************************************************************************
**
** Y8960 cartridge - the OPL2EX block: YM3812 with ADPCM-B.
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
#include "Y8960Opl2ex.h"
#include "AudioMixer.h"
#include <stdlib.h>

/* The first circuit answers at MSX-AUDIO's own addresses, the second below it.
** Unlike the OPLL and DCSG blocks this block reads back, so both circuits
** register a read as well. */
#define Y8960_OPL20_ADDRESS 0xC0
#define Y8960_OPL20_DATA    0xC1
#define Y8960_OPL21_ADDRESS 0xC2
#define Y8960_OPL21_DATA    0xC3

typedef struct {
    int          deviceHandle;
    Y8960Opl2ex* chip;
} RomMapperY8960Opl2ex;

static RomMapperY8960Opl2ex* theOpl2ex = NULL;

static int circuitOf(UInt16 port)
{
    return (port == Y8960_OPL21_ADDRESS || port == Y8960_OPL21_DATA) ? 1 : 0;
}

static int portOf(UInt16 port)
{
    return (port == Y8960_OPL20_DATA || port == Y8960_OPL21_DATA) ? 1 : 0;
}

static Y8960IoBlock blockOf(int circuit)
{
    return circuit ? Y8960_IO_OPL21 : Y8960_IO_OPL20;
}

static void writeIo(RomMapperY8960Opl2ex* rm, UInt16 port, UInt8 value)
{
    int circuit = circuitOf(port);

    if (!y8960IoEnabled(blockOf(circuit))) {
        return;
    }

    y8960Opl2exWrite(rm->chip, circuit, portOf(port), value);
}

static UInt8 readIo(RomMapperY8960Opl2ex* rm, UInt16 port)
{
    int circuit = circuitOf(port);

    if (!y8960IoEnabled(blockOf(circuit))) {
        return 0xFF;
    }

    return y8960Opl2exRead(rm->chip, circuit, portOf(port));
}

/* The tunnel is not gated: the enablers live in the same window and could not
** be reached otherwise. */
static void tunnel0(void* ref, int port, UInt8 value)
{
    RomMapperY8960Opl2ex* rm = (RomMapperY8960Opl2ex*)ref;

    y8960Opl2exWrite(rm->chip, 0, port, value);
}

static void tunnel1(void* ref, int port, UInt8 value)
{
    RomMapperY8960Opl2ex* rm = (RomMapperY8960Opl2ex*)ref;

    y8960Opl2exWrite(rm->chip, 1, port, value);
}

static void reset(RomMapperY8960Opl2ex* rm)
{
    y8960Opl2exReset(rm->chip);
}

static void saveState(RomMapperY8960Opl2ex* rm)
{
    y8960Opl2exSaveState(rm->chip);
}

static void loadState(RomMapperY8960Opl2ex* rm)
{
    y8960Opl2exLoadState(rm->chip);
}

static void destroy(RomMapperY8960Opl2ex* rm)
{
    y8960UnregisterTunnel(Y8960_TUN_OPL20, rm);
    y8960UnregisterTunnel(Y8960_TUN_OPL21, rm);

    ioPortUnregister(Y8960_OPL20_ADDRESS, rm);
    ioPortUnregister(Y8960_OPL20_DATA, rm);
    ioPortUnregister(Y8960_OPL21_ADDRESS, rm);
    ioPortUnregister(Y8960_OPL21_DATA, rm);

    y8960Opl2exDestroy(rm->chip);

    deviceManagerUnregister(rm->deviceHandle);

    free(rm);

    theOpl2ex = NULL;
}

void romMapperY8960Opl2exDestroy(void)
{
    if (theOpl2ex != NULL) {
        destroy(theOpl2ex);
    }
}

int romMapperY8960Opl2exCreate(void)
{
    DeviceCallbacks callbacks = { destroy, reset, saveState, loadState };
    RomMapperY8960Opl2ex* rm = (RomMapperY8960Opl2ex*)calloc(1, sizeof(RomMapperY8960Opl2ex));

    rm->deviceHandle = deviceManagerRegister(ROM_Y8960OPL2EX, &callbacks, rm);

    theOpl2ex = rm;

    /* One object holds both circuits because they share the sample RAM. */
    rm->chip = y8960Opl2exCreate(boardGetMixer());

    ioPortRegister(Y8960_OPL20_ADDRESS, readIo, writeIo, rm);
    ioPortRegister(Y8960_OPL20_DATA, readIo, writeIo, rm);
    ioPortRegister(Y8960_OPL21_ADDRESS, readIo, writeIo, rm);
    ioPortRegister(Y8960_OPL21_DATA, readIo, writeIo, rm);

    y8960RegisterTunnel(Y8960_TUN_OPL20, tunnel0, rm);
    y8960RegisterTunnel(Y8960_TUN_OPL21, tunnel1, rm);

    reset(rm);

    return 1;
}
