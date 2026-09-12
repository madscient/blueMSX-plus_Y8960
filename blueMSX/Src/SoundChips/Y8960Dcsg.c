/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/SoundChips/SN76489.c,v $
**
** $Revision: 1.21 $
**
** $Date: 2009-04-10 04:38:10 $
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2006 Daniel Vik
**
** Forked for the Y8960 cartridge, 2026 by madscient.
** The cartridge carries a DCSG-equivalent circuit (sn76489_audio in the
** hardware), not a TI SN76489, so it is emulated as its own chip rather than
** sharing the machine's. Divergence in behaviour is expected as the hardware
** is finished; keeping them separate is what makes that possible.
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
#include "Y8960Dcsg.h"
#include "IoPort.h"
#include "SaveState.h"
#include "DebugDeviceManager.h"
#include "Language.h"
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <string.h>
#include <math.h>
#include <limits.h>




#define FB_BBCMICRO  0x0005
#define FB_SC3000    0x0006
#define FB_SEGA      0x0009
#define FB_COLECO    0x0003

#define SRW_SEGA     16
#define SRW_COLECO   15

#define VOL_TRUNC    0
#define VOL_FULL     0

#define SR_INIT       0x4000
#define PSG_CUTOFF    0x6

static int VoltTables[2][16] = 
{
    { 9897, 9897, 9897, 8432, 6912, 5514, 4482, 3584, 2851, 2196, 1764, 1365, 1065,  832,  666, 0 },
    { 9897, 7867, 6248, 4962, 3937, 3127, 2487, 1978, 1567, 1247,  992,  783,  627,  496,  392, 0 }
};

#define DELTA_CLOCK  ((float)3579545 / 16 / 44100)

struct Y8960DcsgChip {
    /* Framework params */
    Mixer* mixer;
    Int32  handle;
    Int32  debugHandle;

    /* Configuration params */
    int voltTableIdx;
    int whiteNoiseFeedback;
    int shiftRegisterWidth;
    
    /* State params */
    float clock;

    int regs[8];
    int latch;
    int shiftReg;
    int noiseFreq;
    
    int toneFrequency[4];
    int toneFlipFlop[4];
    float toneInterpol[4];

    /* Filter params */
    Int32  ctrlVolume;
    Int32  oldSampleVolume;
    Int32  daVolume;

    /* Audio buffer */
    Int32  buffer[AUDIO_MONO_BUFFER_SIZE];
};


static Int32* y8960DcsgSync(void* ref, UInt32 count);


void y8960DcsgLoadState(Y8960DcsgChip* sn76489)
{
    SaveState* state = saveStateOpenForRead("y8960dcsg");
    char tag[32];
    int i;
    
    sn76489->latch            = saveStateGet(state, "latch",           0);
    sn76489->shiftReg         = saveStateGet(state, "shiftReg",        0);
    sn76489->noiseFreq        = saveStateGet(state, "noiseFreq",       1);

    sn76489->ctrlVolume       = saveStateGet(state, "ctrlVolume",      0);
    sn76489->oldSampleVolume  = saveStateGet(state, "oldSampleVolume", 0);
    sn76489->daVolume         = saveStateGet(state, "daVolume",        0);

    for (i = 0; i < 8; i++) {
        sprintf(tag, "reg%d", i);
        sn76489->regs[i] = saveStateGet(state, tag, 0);
    }

    for (i = 0; i < 4; i++) {
        sprintf(tag, "toneFrequency%d", i);
        sn76489->toneFrequency[i] = saveStateGet(state, tag, 0);

        sprintf(tag, "toneFlipFlop%d", i);
        sn76489->toneFlipFlop[i] = saveStateGet(state, tag, 0);
    }

    saveStateClose(state);
}

void y8960DcsgSaveState(Y8960DcsgChip* sn76489)
{
    SaveState* state = saveStateOpenForWrite("y8960dcsg");
    char tag[32];
    int i;

    saveStateSet(state, "latch",           sn76489->latch);
    saveStateSet(state, "shiftReg",        sn76489->shiftReg);
    saveStateSet(state, "noiseFreq",       sn76489->noiseFreq);

    saveStateSet(state, "ctrlVolume",      sn76489->ctrlVolume);
    saveStateSet(state, "oldSampleVolume", sn76489->oldSampleVolume);
    saveStateSet(state, "daVolume",        sn76489->daVolume);

    for (i = 0; i < 8; i++) {
        sprintf(tag, "reg%d", i);
        saveStateSet(state, tag, sn76489->regs[i]);
    }

    for (i = 0; i < 4; i++) {
        sprintf(tag, "toneFrequency%d", i);
        saveStateSet(state, tag, sn76489->toneFrequency[i]);

        sprintf(tag, "toneFlipFlop%d", i);
        saveStateSet(state, tag, sn76489->toneFlipFlop[i]);
        
        sn76489->toneInterpol[i] = 0;
    }

    sn76489->clock = 0;

    saveStateClose(state);
}

static void getDebugInfo(Y8960DcsgChip* sn76489, DbgDevice* dbgDevice)
{
    DbgRegisterBank* regBank;
    int i;

    regBank = dbgDeviceAddRegisterBank(dbgDevice, langDbgRegs(), 8);

    for (i = 0; i < 4; i++) {
        char reg[4];
        sprintf(reg, "V%d", i + 1);
        dbgRegisterBankAddRegister(regBank,  i, reg, 8, sn76489->regs[2 * i + 1] & 0x0f);
    }
    
    for (i = 0; i < 4; i++) {
        char reg[4];
        sprintf(reg, "T%d", i + 1);
        if (i < 3) {
            dbgRegisterBankAddRegister(regBank,  i + 4, reg, 16, sn76489->regs[2 * i] & 0x03ff);
        }
        else {
            dbgRegisterBankAddRegister(regBank,  i + 4, reg, 8, sn76489->regs[2 * i] & 0x03);
        }
    }

}

void y8960DcsgDestroy(Y8960DcsgChip* sn76489)
{
    debugDeviceUnregister(sn76489->debugHandle);
    mixerUnregisterChannel(sn76489->mixer, sn76489->handle);
    free(sn76489);
}

void y8960DcsgReset(Y8960DcsgChip* sn76489)
{
    Y8960DcsgChip* p = sn76489;
    int i;

    for( i = 0; i <= 3; i++ )
    {
        p->regs[2 * i]      = 1;
        p->regs[2 * i + 1]  = 0xf;
        p->noiseFreq        = 0x10;
        p->toneFrequency[i] = 0;
        p->toneFlipFlop[i]  = 1;
        p->toneInterpol[i]  = FLT_MIN;
    }

    p->clock    = 0;
    p->latch    = 0;
    p->shiftReg = 1 << (sn76489->shiftRegisterWidth - 1);
}

Y8960DcsgChip* y8960DcsgCreate(Mixer* mixer, const char* name)
{
    DebugCallbacks dbgCallbacks = { getDebugInfo, NULL, NULL, NULL };
    Y8960DcsgChip* sn76489 = (Y8960DcsgChip*)calloc(1, sizeof(Y8960DcsgChip));

    sn76489->mixer = mixer;

    sn76489->handle = mixerRegisterChannel(mixer, MIXER_CHANNEL_Y8960, 0, y8960DcsgSync, NULL, sn76489);
    sn76489->debugHandle = debugDeviceRegister(DBGTYPE_AUDIO, name, &dbgCallbacks, sn76489);


    sn76489->voltTableIdx       = VOL_FULL;
    sn76489->whiteNoiseFeedback = FB_COLECO;
    sn76489->shiftRegisterWidth = SRW_COLECO;

    y8960DcsgReset(sn76489);

    return sn76489;
}

void y8960DcsgWriteData(Y8960DcsgChip* sn76489, UInt16 ioPort, UInt8 data)
{
    Y8960DcsgChip* p = sn76489;

    mixerSync(p->mixer);

    if (data & 0x80) {
        p->latch = ( data >> 4 ) & 0x07;
        p->regs[p->latch] = (p->regs[p->latch] & 0x3f0) | (data & 0x0f);
    } 
    else {
        if ((p->latch & 1) == 0 && p->latch < 5) {
            p->regs[p->latch] = (p->regs[p->latch] & 0x0f) | ((data & 0x3f) << 4);
        }
        else {
            p->regs[p->latch] = data & 0x0f;
        }
    }
    switch (p->latch) {
    case 0:
    case 2:
    case 4:
        if (p->latch == 4 && (p->regs[6] & 3) == 0x03) {
            p->noiseFreq = p->regs[4];
        }
        break;
    case 6:
        p->shiftReg = SR_INIT;
        if ((p->regs[6] & 3) == 0x03) {
            p->noiseFreq = p->regs[4];
        }
        else {
            p->noiseFreq = 0x10 << (p->regs[6] & 0x3);
        }
        break;
    }
}

static Int32* y8960DcsgSync(void* ref, UInt32 count)
{
    Y8960DcsgChip* p = (Y8960DcsgChip*)ref;
    int clocksPerSample;
    UInt32 j;
    int i;

    for(j = 0; j < count; j++) {
        Int32 sampleVolume = 0;

        for (i = 0; i < 3; i++) {
            if (p->toneInterpol[i] > FLT_MIN) {
                sampleVolume += (int)(VoltTables[p->voltTableIdx][p->regs[2 * i + 1]] * p->toneInterpol[i]);
            }
            else {
                sampleVolume += VoltTables[p->voltTableIdx][p->regs[2 * i + 1]] * p->toneFlipFlop[i];
            }
        }

        sampleVolume += VoltTables[p->voltTableIdx][p->regs[7]] * ( p->shiftReg & 0x1 ) * 2;

        /* Perform DC offset filtering */
        p->ctrlVolume = sampleVolume - p->oldSampleVolume + 0x3fe7 * p->ctrlVolume / 0x4000;
        p->oldSampleVolume = sampleVolume;

        /* Perform simple 1 pole low pass IIR filtering */
        p->daVolume += 2 * (p->ctrlVolume - p->daVolume) / 3;
        
        /* Store calclulated sample value */
        p->buffer[j] = 4 * p->daVolume;

        /* Increment clock by 1 sample length */
        p->clock += DELTA_CLOCK;
        clocksPerSample = (int)p->clock;
        p->clock -= clocksPerSample;
    
        for (i = 0; i <= 2; i++) {
            p->toneFrequency[i] -= clocksPerSample;
        }

        if (p->noiseFreq == 0x80) {
            p->toneFrequency[3] = p->toneFrequency[2];
        }
        else {
            p->toneFrequency[3] -= clocksPerSample;
        }
    
        for (i = 0; i <= 2; i++) {
            if (p->regs[2 * i] == 0) {
                p->toneFlipFlop[i] = 1;
                p->toneInterpol[i] = FLT_MIN;
                p->toneFrequency[i] = 0;
            }
            else if (p->toneFrequency[i] <= 0) {
                if (p->regs[i * 2] > PSG_CUTOFF) {
                    p->toneInterpol[i] = (clocksPerSample - p->clock + 2 * p->toneFrequency[i]) * p->toneFlipFlop[i] / (clocksPerSample + p->clock);
                    p->toneFlipFlop[i] = -p->toneFlipFlop[i];
                }
                else {
                    p->toneFlipFlop[i] = 1;
                    p->toneInterpol[i] = FLT_MIN;
                }
                p->toneFrequency[i] += p->regs[i*2] * (clocksPerSample / p->regs[i*2] + 1);
            }
            else {
                p->toneInterpol[i] = FLT_MIN;
            }
        }

        if (p->noiseFreq == 0) {
            p->toneFlipFlop[3] = 1;
            p->toneFrequency[3] = 0;
        }
        else if (p->toneFrequency[3] <= 0) {
            p->toneFlipFlop[3] = -p->toneFlipFlop[3];
            if (p->noiseFreq != 0x80) {
                p->toneFrequency[3] += p->noiseFreq * (clocksPerSample / p->noiseFreq + 1);
            }
            if (p->toneFlipFlop[3] == 1) {
                int feedback;
                if ( p->regs[6] & 0x4 ) {
                    feedback = p->shiftReg & p->whiteNoiseFeedback;
                    feedback ^= feedback >> 8;
                    feedback ^= feedback >> 4;
                    feedback ^= feedback >> 2;
                    feedback ^= feedback >> 1;
                    feedback &= 1;
                } else {
                    feedback = p->shiftReg & 1;
                }

                p->shiftReg = (p->shiftReg >> 1) | (feedback << (p->shiftRegisterWidth - 1));
            }
        }
    }

    return p->buffer;
}

