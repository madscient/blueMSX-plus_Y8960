/*****************************************************************************
**
** Y8960 cartridge - the SCC block: Konami SCC plus the bank mapper, the I/O
** enablers and the memory mapped window.
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
#include "SlotManager.h"
#include "SaveState.h"
#include <stdlib.h>
#include <string.h>

/* 8kB x 32 banks. Banks 0-15 are ROM and 16-31 are RAM, which is what the
** firmware assumes; the RTL carries no such split because it is unfinished. */
#define Y8960_BANKS      32
#define Y8960_BANK_SIZE  0x2000
#define Y8960_ROM_BANKS  16
#define Y8960_MEM_SIZE   (Y8960_BANKS * Y8960_BANK_SIZE)

#define Y8960_REGIONS    4

typedef struct {
    int    deviceHandle;
    UInt8* memory;
    int    slot;
    int    sslot;
    int    startPage;
    /* Six bits are kept even though only five reach an address: the value
    ** 3Fh selects the SCC window instead of a bank. */
    UInt8  bankReg[Y8960_REGIONS];
    UInt8  ramMode;
} RomMapperY8960Scc;

static void bankSwitch(RomMapperY8960Scc* rm, int region)
{
    int    bank     = rm->bankReg[region] & 0x1F;
    UInt8* bankData = rm->memory + bank * Y8960_BANK_SIZE;
    /* Region 0 carries the RAM mode and bank registers, so its writes have
    ** to come through the callback even when a RAM bank is mapped there. */
    int    writable = rm->ramMode && region != 0 && bank >= Y8960_ROM_BANKS;

    slotMapPage(rm->slot, rm->sslot, rm->startPage + region, bankData, 1, writable);
}

static void bankSwitchAll(RomMapperY8960Scc* rm)
{
    int region;

    for (region = 0; region < Y8960_REGIONS; region++) {
        bankSwitch(rm, region);
    }
}

static void reset(RomMapperY8960Scc* rm)
{
    int region;

    rm->ramMode = 0;
    for (region = 0; region < Y8960_REGIONS; region++) {
        rm->bankReg[region] = (UInt8)region;
    }

    bankSwitchAll(rm);
}

static void saveState(RomMapperY8960Scc* rm)
{
    SaveState* state = saveStateOpenForWrite("y8960Scc");

    saveStateSet(state, "ramMode", rm->ramMode);
    saveStateSet(state, "bankReg0", rm->bankReg[0]);
    saveStateSet(state, "bankReg1", rm->bankReg[1]);
    saveStateSet(state, "bankReg2", rm->bankReg[2]);
    saveStateSet(state, "bankReg3", rm->bankReg[3]);
    saveStateSetBuffer(state, "ram", rm->memory + Y8960_ROM_BANKS * Y8960_BANK_SIZE,
                       (Y8960_BANKS - Y8960_ROM_BANKS) * Y8960_BANK_SIZE);

    saveStateClose(state);
}

static void loadState(RomMapperY8960Scc* rm)
{
    SaveState* state = saveStateOpenForRead("y8960Scc");

    rm->ramMode     = (UInt8)saveStateGet(state, "ramMode", 0);
    rm->bankReg[0]  = (UInt8)saveStateGet(state, "bankReg0", 0);
    rm->bankReg[1]  = (UInt8)saveStateGet(state, "bankReg1", 1);
    rm->bankReg[2]  = (UInt8)saveStateGet(state, "bankReg2", 2);
    rm->bankReg[3]  = (UInt8)saveStateGet(state, "bankReg3", 3);
    saveStateGetBuffer(state, "ram", rm->memory + Y8960_ROM_BANKS * Y8960_BANK_SIZE,
                       (Y8960_BANKS - Y8960_ROM_BANKS) * Y8960_BANK_SIZE);

    saveStateClose(state);

    bankSwitchAll(rm);
}

static void destroy(RomMapperY8960Scc* rm)
{
    slotUnregister(rm->slot, rm->sslot, rm->startPage);
    deviceManagerUnregister(rm->deviceHandle);

    free(rm->memory);
    free(rm);
}

static void setBank(RomMapperY8960Scc* rm, int region, UInt8 value)
{
    value &= 0x3F;

    if (rm->bankReg[region] == value) {
        return;
    }

    rm->bankReg[region] = value;
    bankSwitch(rm, region);
}

static UInt8 read(RomMapperY8960Scc* rm, UInt16 address)
{
    int bank = rm->bankReg[address >> 13] & 0x1F;

    return rm->memory[bank * Y8960_BANK_SIZE + (address & 0x1FFF)];
}

static void write(RomMapperY8960Scc* rm, UInt16 address, UInt8 value)
{
    int region = address >> 13;
    int bank;

    /* 4?FBh with ? = 8..F, the only register visible in both modes. */
    if (address >= 0x0800 && address < 0x1000 && (address & 0xFF) == 0xFB) {
        UInt8 newMode = value & 0x01;
        if (rm->ramMode != newMode) {
            rm->ramMode = newMode;
            bankSwitchAll(rm);
        }
        return;
    }

    if (rm->ramMode) {
        /* 4?FCh-4?FFh select the banks while RAM mode is on. */
        if (address >= 0x0800 && address < 0x1000 && (address & 0xFF) >= 0xFC) {
            setBank(rm, (address & 0x03), value);
            return;
        }
    }
    else {
        /* Konami SCC layout: 5000, 7000, 9000 and B000, each 800h wide. */
        if ((address & 0x1800) == 0x1000) {
            setBank(rm, region, value);
            return;
        }
    }

    bank = rm->bankReg[region] & 0x1F;
    if (rm->ramMode && bank >= Y8960_ROM_BANKS) {
        rm->memory[bank * Y8960_BANK_SIZE + (address & 0x1FFF)] = value;
    }
}

int romMapperY8960SccCreate(const char* filename, UInt8* romData,
                            int size, int slot, int sslot, int startPage)
{
    DeviceCallbacks callbacks = { destroy, reset, saveState, loadState };
    RomMapperY8960Scc* rm = (RomMapperY8960Scc*)calloc(1, sizeof(RomMapperY8960Scc));

    rm->deviceHandle = deviceManagerRegister(ROM_Y8960SCC, &callbacks, rm);
    slotRegister(slot, sslot, startPage, Y8960_REGIONS, read, read, write, destroy, rm);

    rm->memory    = (UInt8*)calloc(1, Y8960_MEM_SIZE);
    rm->slot      = slot;
    rm->sslot     = sslot;
    rm->startPage = startPage;

    if (romData != NULL && size > 0) {
        int romSize = size;
        if (romSize > Y8960_ROM_BANKS * Y8960_BANK_SIZE) {
            romSize = Y8960_ROM_BANKS * Y8960_BANK_SIZE;
        }
        memcpy(rm->memory, romData, romSize);
    }

    reset(rm);

    return 1;
}
