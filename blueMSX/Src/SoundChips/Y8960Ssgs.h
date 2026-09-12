/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/SoundChips/AY8910.h,v $
**
** $Revision: 1.13 $
**
** $Date: 2008-03-30 18:38:44 $
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
#ifndef Y8960_SSGS_H
#define Y8960_SSGS_H

#include "MsxTypes.h"
#include "AudioMixer.h"

typedef struct Y8960SsgsChip Y8960SsgsChip;

Y8960SsgsChip* y8960SsgsCreate(Mixer* mixer, const char* name);
void y8960SsgsDestroy(Y8960SsgsChip* chip);

void y8960SsgsReset(Y8960SsgsChip* chip);

/* Write only: the cartridge does not drive a read port. The address covers
** both cores, 00h-1Fh for the first and 20h-3Fh for the second. */
void y8960SsgsWriteAddress(Y8960SsgsChip* chip, UInt8 address);
void y8960SsgsWriteData(Y8960SsgsChip* chip, UInt8 data);

/* The four LED bits the second core's 2Fh carries. Nothing drives them yet;
** they are kept so the debugger can show what software asked for. */
UInt8 y8960SsgsGetLed(Y8960SsgsChip* chip);

void y8960SsgsSaveState(Y8960SsgsChip* chip);
void y8960SsgsLoadState(Y8960SsgsChip* chip);

#endif
