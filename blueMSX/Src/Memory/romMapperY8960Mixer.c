/*****************************************************************************
**
** Y8960 cartridge - the MSX-MIXER block: per-block left and right gain.
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
#include "DebugDeviceManager.h"
#include "Language.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* The addresses the cartridge's I/O port list gives this block. They are the
** only thing about it that is written down: the register map in the manual is
** blank, and the hardware does not decode these addresses yet. Both are
** therefore provisional, and what the registers mean is unknown.
**
** So this block remembers what is written and does nothing with it. Nothing
** reads a gain from here yet, because there is no mapping to read it through;
** wiring one in now would be inventing the part that is missing. */
#define Y8960_MIXER_ADDRESS 0xB6
#define Y8960_MIXER_DATA    0xB7

#define Y8960_MIXER_REGS    256

typedef struct {
    int   deviceHandle;
    int   debugHandle;
    UInt8 address;
    UInt8 regs[Y8960_MIXER_REGS];
} RomMapperY8960Mixer;

static RomMapperY8960Mixer* theMixer = NULL;

/* No enabler bit is assigned to this block: enabler 2 names the OPL2 pair, the
** DCSG pair, the SSGS and the timer, and nothing for the mixer. Its ports are
** therefore not gated. */
static void writeIo(RomMapperY8960Mixer* rm, UInt16 port, UInt8 value)
{
    if (port == Y8960_MIXER_DATA) {
        rm->regs[rm->address] = value;
    }
    else {
        rm->address = value;
    }
}

static void reset(RomMapperY8960Mixer* rm)
{
    rm->address = 0;
    memset(rm->regs, 0, sizeof(rm->regs));
}

static void saveState(RomMapperY8960Mixer* rm)
{
    SaveState* state = saveStateOpenForWrite("y8960mixer");

    saveStateSet      (state, "address", rm->address);
    saveStateSetBuffer(state, "regs",    rm->regs, sizeof(rm->regs));

    saveStateClose(state);
}

static void loadState(RomMapperY8960Mixer* rm)
{
    SaveState* state = saveStateOpenForRead("y8960mixer");

    rm->address = (UInt8)saveStateGet(state, "address", 0);
    saveStateGetBuffer(state, "regs", rm->regs, sizeof(rm->regs));

    saveStateClose(state);
}

/* The debugger is the only way to see these, the block having no read port. */
static void getDebugInfo(RomMapperY8960Mixer* rm, DbgDevice* dbgDevice)
{
    DbgRegisterBank* regBank;
    int r;

    regBank = dbgDeviceAddRegisterBank(dbgDevice, langDbgRegs(), Y8960_MIXER_REGS);

    for (r = 0; r < Y8960_MIXER_REGS; r++) {
        char name[8];
        sprintf(name, "R%.2x", r);
        dbgRegisterBankAddRegister(regBank, r, name, 8, rm->regs[r]);
    }
}

static void destroy(RomMapperY8960Mixer* rm)
{
    debugDeviceUnregister(rm->debugHandle);

    ioPortUnregister(Y8960_MIXER_ADDRESS, rm);
    ioPortUnregister(Y8960_MIXER_DATA, rm);

    deviceManagerUnregister(rm->deviceHandle);

    free(rm);

    theMixer = NULL;
}

void romMapperY8960MixerDestroy(void)
{
    if (theMixer != NULL) {
        destroy(theMixer);
    }
}

int romMapperY8960MixerCreate(void)
{
    DeviceCallbacks callbacks = { destroy, reset, saveState, loadState };
    DebugCallbacks dbgCallbacks = { getDebugInfo, NULL, NULL, NULL };
    RomMapperY8960Mixer* rm = (RomMapperY8960Mixer*)calloc(1, sizeof(RomMapperY8960Mixer));

    rm->deviceHandle = deviceManagerRegister(ROM_Y8960MIXER, &callbacks, rm);

    theMixer = rm;
    rm->debugHandle  = debugDeviceRegister(DBGTYPE_AUDIO, "Y8960 MSX-MIXER", &dbgCallbacks, rm);

    /* Write only. Nothing says the block answers reads, and a device that
    ** drives a port it does not own would pull the bus down for whoever
    ** does. */
    ioPortRegister(Y8960_MIXER_ADDRESS, NULL, writeIo, rm);
    ioPortRegister(Y8960_MIXER_DATA, NULL, writeIo, rm);

    reset(rm);

    return 1;
}
