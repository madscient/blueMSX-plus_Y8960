/*****************************************************************************
**
** Y8960 cartridge - the Y8960 block: declarations shared by the device blocks.
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
#ifndef ROM_MAPPER_Y8960_H
#define ROM_MAPPER_Y8960_H

#include "MsxTypes.h"

/* The I/O enablers sit in the memory mapped window, which belongs to the SCC
** block. Every other block asks whether its own ports are open before
** answering, the way Panasonic's FM-PAC does.
**
** Reset leaves all of them closed. The cartridge has no direct I/O port for
** the enablers, so software has to reach them through the window. */
typedef enum {
    Y8960_IO_OPLL0,     /* 7CH-7DH, enabler 1 bit 0 */
    Y8960_IO_OPLL1,     /* 7AH-7BH, enabler 1 bit 1 */
    Y8960_IO_OPL20,     /* C0H-C1H, enabler 2 bit 0 */
    Y8960_IO_OPL21,     /* C2H-C3H, enabler 2 bit 1 */
    Y8960_IO_DCSG0,     /* 3EH,     enabler 2 bit 2 */
    Y8960_IO_DCSG1,     /* 3FH,     enabler 2 bit 3 */
    Y8960_IO_SSGS,      /* A0H-A2H, enabler 2 bit 4 */
    Y8960_IO_TIMER      /* B0H-B3H, enabler 2 bit 7 */
} Y8960IoBlock;

/* Zero when the SCC block is absent, so a block placed on its own in a
** machine configuration stays silent rather than answering unconditionally. */
int y8960IoEnabled(Y8960IoBlock block);

/* One block per RomType, so a machine configuration can carry them
** independently while the cartridge hardware is still being designed. */
int romMapperY8960OpllexCreate(void);
int romMapperY8960Opl2exCreate(void);
int romMapperY8960SsgsCreate(void);
int romMapperY8960TimerCreate(void);
int romMapperY8960MixerCreate(void);
/* The SCC block carries the cartridge ROM and the bank mapper, so unlike the
** others it is created from the slot entry. */
int romMapperY8960SccCreate(const char* filename, UInt8* romData,
                            int size, int slot, int sslot, int startPage);
int romMapperY8960DcsgCreate(void);

#endif
