/*****************************************************************************
**
** Y8960 cartridge - the OPL2EX block: two YM3812 + ADPCM-B circuits that
** share one sample RAM.
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
#include "Y8960Opl2ex.h"
#include "Y8960Opl2Core.h"

extern "C" {
#include "Board.h"
#include "SaveState.h"
#include "DebugDeviceManager.h"
#include "Language.h"
}

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#define FREQUENCY  3579545
#define SAMPLERATE (FREQUENCY / 72)

#define Y8960_OPL2EX_CIRCUITS 2

/* 256kB, the whole of the cartridge's sample RAM. Which part each circuit
** sees is the sample RAM's business, not this file's. */
#define Y8960_OPL2EX_SAMPLE_RAM (256 * 1024)

/* Bits nothing else in the emulator claims; the MSX-TIMER block took 0x1000
** the same way. One per circuit, so that clearing one does not drop the
** other's pending interrupt. */
static const UInt32 Y8960_OPL2EX_IRQ[Y8960_OPL2EX_CIRCUITS] = { 0x2000, 0x4000 };

/* The same << 1 the machine's own Y8950 port applies, so a block lands where
** the chip it stands in for would. Whether it balances against the other
** Y8960 blocks is unverified; that is the mixer block's question. */
#define Y8960_OPL2EX_GAIN_SHIFT 1

namespace {

struct Circuit {
    Mixer*                 mixer;
    y8960opl2::Y8950       chip;
    std::vector<int32_t>   chipSamples;
    Int32                  handle;
    Int32                  debugHandle;
    UInt32                 mixerRate;
    Int32                  off, o1, o2;
    UInt8                  latchedAddr;
    UInt8                  regCache[256];
    Int32                  buffer[AUDIO_MONO_BUFFER_SIZE];

    Circuit(Mixer* mixer_, const std::string& name, y8960opl2::DeviceConfig& cfg,
            y8960opl2::SampleRam& ram, int index, y8960opl2::EmuTime time)
        : mixer(mixer_)
        , chip(name, cfg, ram, index, Y8960_OPL2EX_IRQ[index], time)
        , handle(0), debugHandle(0), mixerRate(SAMPLERATE)
        , off(0), o1(0), o2(0), latchedAddr(0)
    {
        memset(regCache, 0, sizeof(regCache));
        memset(buffer,   0, sizeof(buffer));
    }
};

} // namespace

struct Y8960Opl2ex {
    Mixer*                          mixer;
    y8960opl2::MSXMotherBoard       mb;
    y8960opl2::DeviceConfig         cfg;
    y8960opl2::SampleRam            ram;
    std::unique_ptr<Circuit>        circuit[Y8960_OPL2EX_CIRCUITS];

    Y8960Opl2ex(Mixer* mixer_)
        : mixer(mixer_), cfg(mb), ram(Y8960_OPL2EX_SAMPLE_RAM)
    {}
};

/* The chip runs at clock/72 whatever the mixer wants, so the extra samples are
** resampled here. Asking the chip for more than the loop consumes turns voices
** into buzz, so the count is worked out before the chip is advanced. */
static Int32* sync(void* ref, UInt32 count)
{
    Circuit& c = *(Circuit*)ref;
    Int32 simOff = c.off;
    int chipNeed;

    if (SAMPLERATE > c.mixerRate) {
        chipNeed = 0;
        for (UInt32 i = 0; i < count; i++) {
            simOff -= SAMPLERATE - (Int32)c.mixerRate;
            chipNeed++;
            if (simOff < 0) {
                simOff += (Int32)c.mixerRate;
                chipNeed++;
            }
        }
    }
    else {
        chipNeed = (int)count;
    }

    if ((int)c.chipSamples.size() < chipNeed) c.chipSamples.resize((size_t)chipNeed);
    c.chip.generateMono(c.chipSamples.data(), (unsigned)chipNeed);

    int32_t* chipBuf = c.chipSamples.data();
    int chipPos = 0;

    for (UInt32 i = 0; i < count; i++) {
        if (SAMPLERATE > c.mixerRate) {
            c.off -= SAMPLERATE - (Int32)c.mixerRate;
            c.o1 = c.o2;
            c.o2 = chipBuf[chipPos++] << Y8960_OPL2EX_GAIN_SHIFT;
            if (c.off < 0) {
                c.off += (Int32)c.mixerRate;
                c.o1 = c.o2;
                c.o2 = chipBuf[chipPos++] << Y8960_OPL2EX_GAIN_SHIFT;
            }
            c.buffer[i] = (c.o1 * (c.off / 256)
                        +  c.o2 * (((Int32)SAMPLERATE - c.off) / 256)) / (SAMPLERATE / 256);
        }
        else {
            c.buffer[i] = chipBuf[chipPos++] << Y8960_OPL2EX_GAIN_SHIFT;
        }
    }

    return c.buffer;
}

static void setRate(void* ref, UInt32 rate)
{
    Circuit& c = *(Circuit*)ref;
    c.mixerRate = rate ? rate : SAMPLERATE;
}

static void getDebugInfo(void* ref, DbgDevice* dbgDevice)
{
    Circuit& c = *(Circuit*)ref;
    DbgRegisterBank* regBank;
    int r;

    regBank = dbgDeviceAddRegisterBank(dbgDevice, langDbgRegs(), 256);

    for (r = 0; r < 256; r++) {
        char name[8];
        sprintf(name, "R%.2x", r);
        dbgRegisterBankAddRegister(regBank, r, name, 8, c.regCache[r]);
    }
}

extern "C" {

void y8960Opl2exReset(Y8960Opl2ex* dev)
{
    y8960opl2::EmuTime time = (y8960opl2::EmuTime)boardSystemTime();

    for (int i = 0; i < Y8960_OPL2EX_CIRCUITS; i++) {
        Circuit& c = *dev->circuit[i];
        c.chip.reset(time);
        c.off = 0;
        c.o1  = 0;
        c.o2  = 0;
        c.latchedAddr = 0;
        memset(c.regCache, 0, sizeof(c.regCache));
    }
}

void y8960Opl2exWrite(Y8960Opl2ex* dev, int index, int port, UInt8 value)
{
    Circuit& c = *dev->circuit[index];

    mixerSync(c.mixer);

    if ((port & 1) == 0) {
        c.latchedAddr = value;
    }
    else {
        c.regCache[c.latchedAddr] = value;
        c.chip.writeReg(c.latchedAddr, value, (y8960opl2::EmuTime)boardSystemTime());
    }
}

UInt8 y8960Opl2exRead(Y8960Opl2ex* dev, int index, int port)
{
    Circuit& c = *dev->circuit[index];

    if ((port & 1) == 0) {
        return c.chip.readStatus((y8960opl2::EmuTime)boardSystemTime());
    }
    return c.chip.readReg(c.latchedAddr, (y8960opl2::EmuTime)boardSystemTime());
}

UInt8 y8960Opl2exPeek(Y8960Opl2ex* dev, int index, int port)
{
    Circuit& c = *dev->circuit[index];

    if ((port & 1) == 0) {
        return c.chip.peekStatus((y8960opl2::EmuTime)boardSystemTime());
    }
    return c.chip.peekReg(c.latchedAddr, (y8960opl2::EmuTime)boardSystemTime());
}

/* One section per circuit, plus one for the sample RAM they share. The
** framework numbers repeated section names, so the two circuits must be
** written and read in the same order. */
void y8960Opl2exSaveState(Y8960Opl2ex* dev)
{
    for (int i = 0; i < Y8960_OPL2EX_CIRCUITS; i++) {
        Circuit& c = *dev->circuit[i];
        SaveState* s = saveStateOpenForWrite("y8960opl2ex");
        saveStateSet      (s, "latchedAddr", c.latchedAddr);
        saveStateSetBuffer(s, "regCache",    c.regCache, sizeof(c.regCache));
        c.chip.blueMsxSaveStateImpl(s);
        saveStateClose(s);
    }

    {
        SaveState* s = saveStateOpenForWrite("y8960opl2ram");
        saveStateSet      (s, "mode", (UInt32)dev->ram.getMode());
        saveStateSetBuffer(s, "ram",  dev->ram.raw(), dev->ram.size());
        saveStateClose(s);
    }
}

void y8960Opl2exLoadState(Y8960Opl2ex* dev)
{
    for (int i = 0; i < Y8960_OPL2EX_CIRCUITS; i++) {
        Circuit& c = *dev->circuit[i];
        SaveState* s = saveStateOpenForRead("y8960opl2ex");
        c.latchedAddr = (UInt8)saveStateGet(s, "latchedAddr", 0);
        saveStateGetBuffer(s, "regCache", c.regCache, sizeof(c.regCache));
        c.chip.blueMsxLoadStateImpl(s);
        saveStateClose(s);
    }

    {
        SaveState* s = saveStateOpenForRead("y8960opl2ram");
        UInt32 mode = saveStateGet(s, "mode", (UInt32)y8960opl2::SampleRam::SHARED);
        dev->ram.setMode((y8960opl2::SampleRam::Mode)mode);
        saveStateGetBuffer(s, "ram", dev->ram.raw(), dev->ram.size());
        saveStateClose(s);
    }
}

Y8960Opl2ex* y8960Opl2exCreate(Mixer* mixer)
{
    static const char* const names[Y8960_OPL2EX_CIRCUITS] = {
        "Y8960 OPL2EX 0", "Y8960 OPL2EX 1"
    };
    DebugCallbacks dbgCallbacks = { getDebugInfo, NULL, NULL, NULL };
    Y8960Opl2ex* dev = new Y8960Opl2ex(mixer);
    y8960opl2::EmuTime time = (y8960opl2::EmuTime)boardSystemTime();

    for (int i = 0; i < Y8960_OPL2EX_CIRCUITS; i++) {
        dev->circuit[i].reset(new Circuit(mixer, names[i], dev->cfg, dev->ram, i, time));

        Circuit& c = *dev->circuit[i];
        c.mixerRate   = mixerGetSampleRate(mixer);
        c.handle      = mixerRegisterChannel(mixer, MIXER_CHANNEL_Y8960, 0,
                                             sync, setRate, &c);
        c.debugHandle = debugDeviceRegister(DBGTYPE_AUDIO, names[i], &dbgCallbacks, &c);
    }

    y8960Opl2exReset(dev);

    return dev;
}

void y8960Opl2exDestroy(Y8960Opl2ex* dev)
{
    for (int i = 0; i < Y8960_OPL2EX_CIRCUITS; i++) {
        Circuit& c = *dev->circuit[i];
        debugDeviceUnregister(c.debugHandle);
        mixerUnregisterChannel(dev->mixer, c.handle);
    }

    delete dev;
}

} // extern "C"
