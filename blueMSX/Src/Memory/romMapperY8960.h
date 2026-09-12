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

/* The window also tunnels writes straight into the sound blocks, at
** 7FEAh-7FF5h. Unlike the direct I/O ports these are NOT gated by the
** enablers: the enablers themselves are only reachable through this window,
** so gating the tunnels would leave no way to open anything.
**
** Inside the window the first circuit of a pair sits at the higher address,
** which is the opposite of the direct I/O ports for OPL2 and DCSG. The two
** orderings were settled separately, for compatibility with earlier designs,
** so neither can be derived from the other. */
typedef enum {
    Y8960_TUN_SSGS,     /* 7FEAh register, 7FEBh value */
    Y8960_TUN_OPL21,    /* 7FECh address,  7FEDh data  */
    Y8960_TUN_OPL20,    /* 7FEEh address,  7FEFh data  */
    Y8960_TUN_DCSG1,    /* 7FF0h, single byte          */
    Y8960_TUN_DCSG0,    /* 7FF1h, single byte          */
    Y8960_TUN_OPLL1,    /* 7FF2h address,  7FF3h data  */
    Y8960_TUN_OPLL0,    /* 7FF4h address,  7FF5h data  */
    Y8960_TUN_COUNT
} Y8960TunnelBlock;

/* port is 0 for the address or register number and 1 for the data; blocks
** that take a single byte only ever see 0. */
typedef void (*Y8960TunnelWrite)(void* ref, int port, UInt8 value);

/* A block registers itself when it is created and clears the entry when it
** is destroyed. Writes to an unclaimed tunnel go nowhere. */
void y8960RegisterTunnel(Y8960TunnelBlock block, Y8960TunnelWrite write, void* ref);
void y8960UnregisterTunnel(Y8960TunnelBlock block, void* ref);

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
