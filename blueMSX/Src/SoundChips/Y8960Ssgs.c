/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/SoundChips/AY8910.c,v $
**
** $Revision: 1.35 $
**
** $Date: 2009-04-10 04:38:10 $
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2006 Daniel Vik
**
** Forked for the Y8960 cartridge, 2026 by madscient.
** The cartridge carries an SSGS (YMZ705/732 equivalent), which is two YM2149
** equivalents with a per-channel pan pot, not a pair of AY-3-8910s. It is
** emulated as its own chip rather than sharing the machine's PSG.
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
#include "Y8960Ssgs.h"
#include "SaveState.h"
#include "DebugDeviceManager.h"
#include "Language.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* = (1 << 28) * 3579545 / 32 / 44100. The SSGS gets 3.579545MHz / 2, which is
** what the machine's own PSG runs at, so the base step carries over
** unchanged. */
#define BASE_PHASE_STEP 0x28959becUL

#define Y8960_SSGS_CORES    2
#define Y8960_SSGS_CHANNELS 3

/* Address bit 5 picks the core and the low five bits the register within it,
** so the two cores sit at 00h-1Fh and 20h-3Fh. */
#define Y8960_SSGS_CORE_OF(addr) (((addr) >> 5) & 1)
#define Y8960_SSGS_SUB_OF(addr)  ((addr) & 0x1F)

/* Registers within a core. 00h-0Dh are the YM2149's; 0Eh-0Fh held its I/O
** ports, which the YMZ parts do not have; 10h-12h are the pan pots this chip
** adds. The second core's 0Fh carries the LED bits instead of an I/O port. */
#define Y8960_SSGS_PAN_FIRST 0x10
#define Y8960_SSGS_PAN_LAST  0x12
#define Y8960_SSGS_LED_REG   0x0F

#define Y8960_SSGS_PAN_MAX    15
#define Y8960_SSGS_PAN_CENTER 8
#define Y8960_SSGS_GAIN_ONE   256

static Int16 voltTable[16];
static Int16 voltEnvTable[32];

/* Pan value to left/right gain, in Y8960_SSGS_GAIN_ONE units.
**
** The data sheet does not give this mapping. EPSGemuEngine and openMSX_Y8960
** both take the one the same-generation YMZ280B uses, and so does this: the
** centre opens both sides fully, moving away from it closes the far side
** linearly, and the last two steps at each end are hard over. It is the
** implementers' reading, not a specification. */
static Int32 panGain[16][2];

static const UInt8 regMask[14] = {
    0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0x3f,
    0x1f, 0x1f, 0x1f, 0xff, 0xff, 0x0f
};

typedef struct {
    UInt8  regs[0x20];

    UInt32 tonePhase[Y8960_SSGS_CHANNELS];
    UInt32 toneStep[Y8960_SSGS_CHANNELS];

    UInt32 noisePhase;
    UInt32 noiseStep;
    UInt32 noiseRand;
    Int16  noiseVolume;

    UInt8  envShape;
    UInt32 envStep;
    UInt32 envPhase;

    UInt8  enable;
    UInt8  ampVolume[Y8960_SSGS_CHANNELS];
    UInt8  pan[Y8960_SSGS_CHANNELS];
} Y8960SsgCore;

struct Y8960SsgsChip {
    Mixer* mixer;
    Int32  handle;
    Int32  debugHandle;

    /* Null on a cartridge, where the first core's 0Eh and 0Fh carry nothing. */
    Y8960SsgsReadCb  ioPortReadCb;
    Y8960SsgsReadCb  ioPortPollCb;
    Y8960SsgsWriteCb ioPortWriteCb;
    void*            ioPortRef;

    UInt8  address;
    UInt8  led;

    Y8960SsgCore core[Y8960_SSGS_CORES];

    /* One filter pair for the mixed output, not one per core: the two cores
    ** share the cartridge's analog stage. */
    Int32  ctrlVolume[2];
    Int32  oldSampleVolume[2];
    Int32  daVolume[2];

    Int32  buffer[AUDIO_STEREO_BUFFER_SIZE];
};

static Int32* y8960SsgsSync(void* ref, UInt32 count);

static void buildPanTable(void)
{
    int pan;

    for (pan = 0; pan <= Y8960_SSGS_PAN_MAX; pan++) {
        Int32 l, r;

        if (pan == Y8960_SSGS_PAN_CENTER) {
            l = Y8960_SSGS_GAIN_ONE;
            r = Y8960_SSGS_GAIN_ONE;
        }
        else if (pan < Y8960_SSGS_PAN_CENTER) {
            l = Y8960_SSGS_GAIN_ONE;
            r = (pan == 0) ? 0
                           : (pan - 1) * Y8960_SSGS_GAIN_ONE / (Y8960_SSGS_PAN_CENTER - 1);
        }
        else {
            l = (Y8960_SSGS_PAN_MAX - pan) * Y8960_SSGS_GAIN_ONE
                / (Y8960_SSGS_PAN_MAX - Y8960_SSGS_PAN_CENTER);
            r = Y8960_SSGS_GAIN_ONE;
        }

        panGain[pan][0] = l;
        panGain[pan][1] = r;
    }
}

static void buildVoltTables(void)
{
    DoubleT v = 0x26a9;
    int i;

    for (i = 15; i >= 0; i--) {
        voltTable[i] = (Int16)v;
        voltEnvTable[2 * i + 0] = (Int16)v;
        voltEnvTable[2 * i + 1] = (Int16)v;
        v *= 0.70794578438413791080221494218943;
    }

    /* The SSGS's cores are YM2149 equivalents, so the envelope runs at the
    ** YM2149's 32 steps rather than the AY-3-8910's 16. */
    v = 0x26a9;
    for (i = 31; i >= 0; i--) {
        voltEnvTable[i] = (Int16)v;
        v *= 0.84139514164519509115274189380029;
    }

    /* Zero the silent level. The pan pots multiply each channel before the
    ** output is filtered, so a channel that is silent but not zero would place
    ** a direct voltage somewhere in the stereo image. */
    for (i = 0; i < 16; i++) {
        voltTable[i] -= voltTable[0];
    }
    for (i = 0; i < 32; i++) {
        voltEnvTable[i] -= voltEnvTable[0];
    }
}

static void updateRegister(Y8960SsgsChip* chip, UInt8 address, UInt8 data)
{
    int core = Y8960_SSGS_CORE_OF(address);
    int sub  = Y8960_SSGS_SUB_OF(address);
    Y8960SsgCore* c = &chip->core[core];
    UInt32 period;

    /* 40h and up is the ADPCM and sequencer area of the YMZ parts, which this
    ** block does not carry. */
    if (address >= 0x40) {
        return;
    }

    if (sub <= Y8960_SSGS_PAN_LAST) {
        mixerSync(chip->mixer);
    }

    if (sub >= Y8960_SSGS_PAN_FIRST && sub <= Y8960_SSGS_PAN_LAST) {
        c->regs[sub] = data & Y8960_SSGS_PAN_MAX;
        c->pan[sub - Y8960_SSGS_PAN_FIRST] = c->regs[sub];
        return;
    }

    if (sub == Y8960_SSGS_LED_REG && core == 1) {
        c->regs[sub] = data & 0x0F;
        chip->led = c->regs[sub];
        return;
    }

    /* 0Eh and 0Fh are the YM2149's I/O ports. They exist only while something
    ** is wired to them, which is the case when the Y8960 is built into a
    ** machine and stands in for its PSG. 13h-1Fh are undefined either way. */
    if (sub == 0x0E || sub == 0x0F) {
        if (core == 0 && chip->ioPortWriteCb != NULL) {
            c->regs[sub] = data;
            chip->ioPortWriteCb(chip->ioPortRef, (UInt16)(sub - 0x0E), data);
        }
        return;
    }

    if (sub >= 14) {
        return;
    }

    data &= regMask[sub];
    c->regs[sub] = data;

    switch (sub) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        period = c->regs[sub & 6] | ((Int32)(c->regs[sub | 1]) << 8);
        c->toneStep[sub >> 1] = period > 0 ? BASE_PHASE_STEP / period : 1 << 31;
        break;

    case 6:
        period = data ? data : 1;
        c->noiseStep = period > 0 ? BASE_PHASE_STEP / period : 1 << 31;
        break;

    case 7:
        c->enable = data;
        break;

    case 8:
    case 9:
    case 10:
        c->ampVolume[sub - 8] = data;
        break;

    case 11:
    case 12:
        period = 16 * (c->regs[11] | ((UInt32)c->regs[12] << 8));
        c->envStep = BASE_PHASE_STEP / (period ? period : 8);
        break;

    case 13:
        if (data < 4) data = 0x09;
        if (data < 8) data = 0x0f;
        c->envShape = data;
        c->envPhase = 0;
        break;
    }
}

void y8960SsgsWriteAddress(Y8960SsgsChip* chip, UInt8 address)
{
    chip->address = address;
}

void y8960SsgsWriteData(Y8960SsgsChip* chip, UInt8 data)
{
    updateRegister(chip, chip->address, data);
}

/* Registers the chip does not have read as all ones rather than as zero: the
** bus ANDs what every device drives, so ones are how a device says it is not
** answering. The registers it does have read back what was written, as the
** PSG this is built from does. */
UInt8 y8960SsgsReadData(Y8960SsgsChip* chip)
{
    int core = Y8960_SSGS_CORE_OF(chip->address);
    int sub  = Y8960_SSGS_SUB_OF(chip->address);

    if (chip->address >= 0x40) {
        return 0xFF;
    }

    if (sub == Y8960_SSGS_LED_REG && core == 1) {
        return (UInt8)(0xF0 | chip->led);
    }

    if (sub == 0x0E || sub == 0x0F) {
        if (core == 0 && chip->ioPortReadCb != NULL) {
            chip->core[0].regs[sub] = chip->ioPortReadCb(chip->ioPortRef, (UInt16)(sub - 0x0E));
            return chip->core[0].regs[sub];
        }
        return 0xFF;
    }

    if (sub < 14) {
        return chip->core[core].regs[sub];
    }

    if (sub >= Y8960_SSGS_PAN_FIRST && sub <= Y8960_SSGS_PAN_LAST) {
        return (UInt8)(0xF0 | chip->core[core].regs[sub]);
    }

    return 0xFF;
}

void y8960SsgsSetIoPort(Y8960SsgsChip* chip, Y8960SsgsReadCb readCb,
                        Y8960SsgsReadCb pollCb, Y8960SsgsWriteCb writeCb, void* ref)
{
    chip->ioPortReadCb  = readCb;
    chip->ioPortPollCb  = pollCb;
    chip->ioPortWriteCb = writeCb;
    chip->ioPortRef     = ref;
}

UInt8 y8960SsgsGetLed(Y8960SsgsChip* chip)
{
    return chip->led;
}

void y8960SsgsReset(Y8960SsgsChip* chip)
{
    int core;
    int i;

    if (chip == NULL) {
        return;
    }

    chip->address = 0;
    chip->led     = 0;

    /* The filters hold the output's recent history. Leaving it in place lets
    ** what the chip was playing before the reset bleed out of it afterwards,
    ** and the time constant is long enough for that to last. */
    for (i = 0; i < 2; i++) {
        chip->ctrlVolume[i]      = 0;
        chip->oldSampleVolume[i] = 0;
        chip->daVolume[i]        = 0;
    }

    for (core = 0; core < Y8960_SSGS_CORES; core++) {
        Y8960SsgCore* c = &chip->core[core];

        memset(c, 0, sizeof(*c));
        c->noiseRand   = 1;
        c->noiseVolume = 1;

        for (i = 0; i < 14; i++) {
            updateRegister(chip, (UInt8)((core << 5) | i), 0);
        }
        /* Centre, not the zero the other registers clear to. Nothing
        ** documents the reset value, and zero is hard left under this law; on
        ** an MSX2++ the SSGS is the machine's own PSG, so that would send
        ** every tune written for a PSG to the left speaker alone.
        ** Provisional until the hardware settles (2026-09-12 user decision). */
        for (i = Y8960_SSGS_PAN_FIRST; i <= Y8960_SSGS_PAN_LAST; i++) {
            updateRegister(chip, (UInt8)((core << 5) | i), Y8960_SSGS_PAN_CENTER);
        }

        /* The GPIO is cleared the way the PSG this stands in for clears it, so
        ** that whatever hangs off it starts in the same state. It does nothing
        ** when no GPIO is attached. */
        updateRegister(chip, (UInt8)((core << 5) | 0x0E), 0);
        updateRegister(chip, (UInt8)((core << 5) | 0x0F), 0);
    }
}

static void getDebugInfo(Y8960SsgsChip* chip, DbgDevice* dbgDevice)
{
    DbgRegisterBank* regBank;
    int core;
    int sub;
    int index = 0;

    /* 00h-0Dh and 10h-12h per core, plus the second core's LED at 2Fh. */
    regBank = dbgDeviceAddRegisterBank(dbgDevice, langDbgRegs(), 2 * (14 + 3) + 1);

    for (core = 0; core < Y8960_SSGS_CORES; core++) {
        for (sub = 0; sub < 14; sub++) {
            char name[8];
            sprintf(name, "R%.2x", (core << 5) | sub);
            dbgRegisterBankAddRegister(regBank, index++, name, 8, chip->core[core].regs[sub]);
        }
        for (sub = Y8960_SSGS_PAN_FIRST; sub <= Y8960_SSGS_PAN_LAST; sub++) {
            char name[8];
            sprintf(name, "R%.2x", (core << 5) | sub);
            dbgRegisterBankAddRegister(regBank, index++, name, 8, chip->core[core].regs[sub]);
        }
    }

    dbgRegisterBankAddRegister(regBank, index, "R2f", 8, chip->led);
}

void y8960SsgsSaveState(Y8960SsgsChip* chip)
{
    SaveState* state = saveStateOpenForWrite("y8960ssgs");
    char tag[32];
    int core;
    int i;

    saveStateSet(state, "address", chip->address);
    saveStateSet(state, "led",     chip->led);

    for (i = 0; i < 2; i++) {
        sprintf(tag, "ctrlVolume%d", i);
        saveStateSet(state, tag, chip->ctrlVolume[i]);
        sprintf(tag, "oldSampleVolume%d", i);
        saveStateSet(state, tag, chip->oldSampleVolume[i]);
        sprintf(tag, "daVolume%d", i);
        saveStateSet(state, tag, chip->daVolume[i]);
    }

    for (core = 0; core < Y8960_SSGS_CORES; core++) {
        Y8960SsgCore* c = &chip->core[core];

        sprintf(tag, "c%d_regs", core);
        saveStateSetBuffer(state, tag, c->regs, sizeof(c->regs));

        for (i = 0; i < Y8960_SSGS_CHANNELS; i++) {
            sprintf(tag, "c%d_tonePhase%d", core, i);
            saveStateSet(state, tag, c->tonePhase[i]);
            sprintf(tag, "c%d_toneStep%d", core, i);
            saveStateSet(state, tag, c->toneStep[i]);
        }

        sprintf(tag, "c%d_noisePhase", core);  saveStateSet(state, tag, c->noisePhase);
        sprintf(tag, "c%d_noiseStep", core);   saveStateSet(state, tag, c->noiseStep);
        sprintf(tag, "c%d_noiseRand", core);   saveStateSet(state, tag, c->noiseRand);
        sprintf(tag, "c%d_noiseVolume", core); saveStateSet(state, tag, c->noiseVolume);
        sprintf(tag, "c%d_envShape", core);    saveStateSet(state, tag, c->envShape);
        sprintf(tag, "c%d_envStep", core);     saveStateSet(state, tag, c->envStep);
        sprintf(tag, "c%d_envPhase", core);    saveStateSet(state, tag, c->envPhase);
    }

    saveStateClose(state);
}

void y8960SsgsLoadState(Y8960SsgsChip* chip)
{
    SaveState* state = saveStateOpenForRead("y8960ssgs");
    char tag[32];
    int core;
    int i;

    chip->address = (UInt8)saveStateGet(state, "address", 0);
    chip->led     = (UInt8)saveStateGet(state, "led", 0);

    for (i = 0; i < 2; i++) {
        sprintf(tag, "ctrlVolume%d", i);
        chip->ctrlVolume[i] = saveStateGet(state, tag, 0);
        sprintf(tag, "oldSampleVolume%d", i);
        chip->oldSampleVolume[i] = saveStateGet(state, tag, 0);
        sprintf(tag, "daVolume%d", i);
        chip->daVolume[i] = saveStateGet(state, tag, 0);
    }

    for (core = 0; core < Y8960_SSGS_CORES; core++) {
        Y8960SsgCore* c = &chip->core[core];

        sprintf(tag, "c%d_regs", core);
        saveStateGetBuffer(state, tag, c->regs, sizeof(c->regs));

        for (i = 0; i < Y8960_SSGS_CHANNELS; i++) {
            sprintf(tag, "c%d_tonePhase%d", core, i);
            c->tonePhase[i] = saveStateGet(state, tag, 0);
            sprintf(tag, "c%d_toneStep%d", core, i);
            c->toneStep[i] = saveStateGet(state, tag, 0);
        }

        sprintf(tag, "c%d_noisePhase", core);  c->noisePhase  = saveStateGet(state, tag, 0);
        sprintf(tag, "c%d_noiseStep", core);   c->noiseStep   = saveStateGet(state, tag, 0);
        sprintf(tag, "c%d_noiseRand", core);   c->noiseRand   = saveStateGet(state, tag, 1);
        sprintf(tag, "c%d_noiseVolume", core); c->noiseVolume = (Int16)saveStateGet(state, tag, 1);
        sprintf(tag, "c%d_envShape", core);    c->envShape    = (UInt8)saveStateGet(state, tag, 0);
        sprintf(tag, "c%d_envStep", core);     c->envStep     = saveStateGet(state, tag, 0);
        sprintf(tag, "c%d_envPhase", core);    c->envPhase    = saveStateGet(state, tag, 0);

        /* Derived from regs, so they are rebuilt rather than saved. */
        c->enable = c->regs[7];
        for (i = 0; i < Y8960_SSGS_CHANNELS; i++) {
            c->ampVolume[i] = c->regs[8 + i];
            c->pan[i]       = c->regs[Y8960_SSGS_PAN_FIRST + i];
        }
    }

    saveStateClose(state);
}

Y8960SsgsChip* y8960SsgsCreate(Mixer* mixer, const char* name)
{
    DebugCallbacks dbgCallbacks = { getDebugInfo, NULL, NULL, NULL };
    Y8960SsgsChip* chip = (Y8960SsgsChip*)calloc(1, sizeof(Y8960SsgsChip));

    buildVoltTables();
    buildPanTable();

    chip->mixer = mixer;

    /* Six channels out of one stereo device: the pan pots place each of them,
    ** so the mixer cannot be left to do it. */
    chip->handle      = mixerRegisterChannel(mixer, MIXER_CHANNEL_Y8960, 1, y8960SsgsSync, NULL, chip);
    chip->debugHandle = debugDeviceRegister(DBGTYPE_AUDIO, name, &dbgCallbacks, chip);

    y8960SsgsReset(chip);

    return chip;
}

void y8960SsgsDestroy(Y8960SsgsChip* chip)
{
    debugDeviceUnregister(chip->debugHandle);
    mixerUnregisterChannel(chip->mixer, chip->handle);

    free(chip);
}

static Int32* y8960SsgsSync(void* ref, UInt32 count)
{
    Y8960SsgsChip* chip = (Y8960SsgsChip*)ref;
    UInt32 index;

    for (index = 0; index < count; index++) {
        Int32 sampleVolumeL = 0;
        Int32 sampleVolumeR = 0;
        int core;

        for (core = 0; core < Y8960_SSGS_CORES; core++) {
            Y8960SsgCore* c = &chip->core[core];
            Int16 envVolume;
            int channel;

            /* Update noise generator */
            c->noisePhase += c->noiseStep;
            while (c->noisePhase >> 28) {
                c->noisePhase  -= 0x10000000;
                c->noiseVolume ^= ((c->noiseRand + 1) >> 1) & 1;
                c->noiseRand    = (c->noiseRand ^ (0x28000 * (c->noiseRand & 1))) >> 1;
            }

            /* Update envelope phase */
            c->envPhase += c->envStep;
            if ((c->envShape & 1) && (c->envPhase >> 28)) {
                c->envPhase = 0x10000000;
            }

            /* Calculate envelope volume */
            envVolume = (Int16)((c->envPhase >> 23) & 0x1f);
            if (((c->envPhase >> 27) & (c->envShape + 1) ^ (~c->envShape >> 1)) & 2) {
                envVolume ^= 0x1f;
            }

            for (channel = 0; channel < Y8960_SSGS_CHANNELS; channel++) {
                UInt32 enable = c->enable >> channel;
                UInt32 noiseEnable = ((enable >> 3) | c->noiseVolume) & 1;
                UInt32 phaseStep = (~enable & 1) * c->toneStep[channel];
                UInt32 tonePhase = c->tonePhase[channel];
                UInt32 tone = 0;
                Int32  oversample = 16;
                Int32  sampleVolume;
                UInt8  pan;

                /* Perform 16x oversampling */
                while (oversample--) {
                    tonePhase += phaseStep;
                    tone += (enable | (tonePhase >> 31)) & noiseEnable;
                }

                c->tonePhase[channel] = tonePhase;

                if (c->ampVolume[channel] & 0x10) {
                    sampleVolume = (Int16)tone * voltEnvTable[envVolume] / 16;
                }
                else {
                    sampleVolume = (Int16)tone * voltTable[c->ampVolume[channel]] / 16;
                }

                pan = c->pan[channel];
                sampleVolumeL += sampleVolume * panGain[pan][0] / Y8960_SSGS_GAIN_ONE;
                sampleVolumeR += sampleVolume * panGain[pan][1] / Y8960_SSGS_GAIN_ONE;
            }
        }

        /* Perform DC offset filtering */
        chip->ctrlVolume[0] = sampleVolumeL - chip->oldSampleVolume[0] + 0x3fe7 * chip->ctrlVolume[0] / 0x4000;
        chip->oldSampleVolume[0] = sampleVolumeL;
        chip->ctrlVolume[1] = sampleVolumeR - chip->oldSampleVolume[1] + 0x3fe7 * chip->ctrlVolume[1] / 0x4000;
        chip->oldSampleVolume[1] = sampleVolumeR;

        /* Perform simple 1 pole low pass IIR filtering */
        chip->daVolume[0] += 2 * (chip->ctrlVolume[0] - chip->daVolume[0]) / 3;
        chip->daVolume[1] += 2 * (chip->ctrlVolume[1] - chip->daVolume[1]) / 3;

        chip->buffer[2 * index + 0] = 9 * chip->daVolume[0];
        chip->buffer[2 * index + 1] = 9 * chip->daVolume[1];
    }

    return chip->buffer;
}
