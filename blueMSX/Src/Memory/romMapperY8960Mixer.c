/*****************************************************************************
**
** Y8960 cartridge - the MSX-MIXER block: one volume per sound block.
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
#include "AudioMixer.h"
#include "DebugDeviceManager.h"
#include "Language.h"
#include "Y8960Pan.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* The mixer has no I/O ports: its five volumes and the master are written
** through the memory mapped window (7FF7h-7FFAh, 7FFDh, 7FFEh), which the SCC block
** decodes and hands here. blueMSX's own mixer has no per-device gain, so
** every sound block asks this one for its gain while rendering, and
** multiplies it in itself. A machine without this block gets unity
** everywhere. */
typedef struct {
    int   deviceHandle;
    int   debugHandle;
    UInt8 volume[Y8960_VOL_COUNT];
    UInt8 master;
} RomMapperY8960Mixer;

static RomMapperY8960Mixer* theMixer = NULL;

void y8960MixerWrite(Y8960Volume channel, UInt8 value)
{
    if (theMixer == NULL || channel >= Y8960_VOL_COUNT) {
        return;
    }
    /* What was rendered so far keeps the old gain. */
    mixerSync(boardGetMixer());
    theMixer->volume[channel] = value;
}

void y8960MixerWriteMaster(UInt8 value)
{
    if (theMixer == NULL) {
        return;
    }
    mixerSync(boardGetMixer());
    theMixer->master = value;
}

/* The master covers the cartridge's own blocks only. The MSX's own sound
** never passes through the card, so blueMSX's other channels stay as they
** are. */
Int32 y8960MixerGain(Y8960Volume channel)
{
    if (theMixer == NULL || channel >= Y8960_VOL_COUNT) {
        return Y8960_GAIN_ONE;
    }
    return y8960VolumeGain(theMixer->volume[channel])
         * y8960VolumeGain(theMixer->master) / Y8960_GAIN_ONE;
}

/* The sheet names 128 as the maximum and the hardware side confirmed it as
** the reset value of the volumes (2026-10-17). The sheet gives none for
** the master; it starts at the maximum too, as anything lower would leave
** every block quieter than its own volume says. A state saved before the
** OPM's volume existed holds four bytes and leaves it at the maximum. */
static void reset(RomMapperY8960Mixer* rm)
{
    memset(rm->volume, 0x80, sizeof(rm->volume));
    rm->master = 0x80;
}

static void saveState(RomMapperY8960Mixer* rm)
{
    SaveState* state = saveStateOpenForWrite("y8960mixer");

    saveStateSetBuffer(state, "volume", rm->volume, sizeof(rm->volume));
    saveStateSet(state, "master", rm->master);

    saveStateClose(state);
}

static void loadState(RomMapperY8960Mixer* rm)
{
    SaveState* state = saveStateOpenForRead("y8960mixer");

    memset(rm->volume, 0x80, sizeof(rm->volume));
    saveStateGetBuffer(state, "volume", rm->volume, sizeof(rm->volume));
    rm->master = (UInt8)saveStateGet(state, "master", 0x80);

    saveStateClose(state);
}

/* The debugger is the only way to see these, the block having no read port. */
static void getDebugInfo(RomMapperY8960Mixer* rm, DbgDevice* dbgDevice)
{
    static const char* const names[Y8960_VOL_COUNT] = {
        "SSG", "OPLL", "OPL2", "SCC", "OPM"
    };
    DbgRegisterBank* regBank;
    int ch;

    regBank = dbgDeviceAddRegisterBank(dbgDevice, langDbgRegs(), Y8960_VOL_COUNT + 1);

    for (ch = 0; ch < Y8960_VOL_COUNT; ch++) {
        dbgRegisterBankAddRegister(regBank, ch, names[ch], 8, rm->volume[ch]);
    }
    dbgRegisterBankAddRegister(regBank, Y8960_VOL_COUNT, "MASTER", 8, rm->master);
}

static void destroy(RomMapperY8960Mixer* rm)
{
    debugDeviceUnregister(rm->debugHandle);

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

    reset(rm);

    return 1;
}
