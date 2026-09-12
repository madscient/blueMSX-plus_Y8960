/*****************************************************************************
** $Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/SoundChips/SN76489.h,v $
**
** $Revision: 1.5 $
**
** $Date: 2008-03-30 18:38:45 $
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
#ifndef Y8960_DCSG_H
#define Y8960_DCSG_H

#include "MsxTypes.h"
#include "AudioMixer.h"

/* Type definitions */
typedef struct Y8960DcsgChip Y8960DcsgChip;

/* Constructor and destructor */
/* name distinguishes the two circuits in the debugger. */
Y8960DcsgChip* y8960DcsgCreate(Mixer* mixer, const char* name);
void y8960DcsgDestroy(Y8960DcsgChip* sn76489);

/* Reset chip */
void y8960DcsgReset(Y8960DcsgChip* sn76489);

/* Register read/write methods */
void y8960DcsgWriteData(Y8960DcsgChip* sn76489, UInt16 port, UInt8 data);

void y8960DcsgLoadState(Y8960DcsgChip* sn76489);
void y8960DcsgSaveState(Y8960DcsgChip* sn76489);

#endif

