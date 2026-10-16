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
** the enablers, so software has to reach them through the window.
**
** A Y8960 built into an MSX2++ has neither window nor enablers, and every
** block answers there without being opened. It keeps its bank memory and its
** SCC; only the window goes.
**
** The specification of 2026-10-16 struck the second OPLL and OPL2 circuits
** and the DCSG out, and may yet move what remains; the window offsets and
** the enabler bits are gathered at the top of romMapperY8960Scc.c. */
typedef enum {
    Y8960_IO_OPLL,      /* 7CH-7DH, enabler 1 bit 0 */
    Y8960_IO_OPL2,      /* C0H-C1H, enabler 2 bit 0 */
    Y8960_IO_SSGS,      /* A0H-A1H, enabler 2 bit 4 */
    Y8960_IO_TIMER      /* B0H-B3H, enabler 2 bit 7 */
} Y8960IoBlock;

/* True on an MSX2++, where the Y8960 is built in and has no window, and when
** the SCC block is absent, which leaves the enablers unreachable. */
int y8960IoEnabled(Y8960IoBlock block);

/* Whether the Y8960 is built into the machine rather than plugged in. The
** board sets it on every machine it builds, before the slots are filled, so a
** machine that follows an MSX2++ does not inherit it. */
void y8960SetBuiltIn(int builtIn);

/* The window also tunnels writes straight into the sound blocks. Unlike the
** direct I/O ports these are NOT gated by the enablers: the enablers
** themselves are only reachable through this window, so gating the tunnels
** would leave no way to open anything. */
typedef enum {
    Y8960_TUN_SSGS,     /* 7FEAh register, 7FEBh value */
    Y8960_TUN_OPL2,     /* 7FEEh address,  7FEFh data  */
    Y8960_TUN_OPLL,     /* 7FF4h address,  7FF5h data  */
    Y8960_TUN_COUNT
} Y8960TunnelBlock;

/* port is 0 for the address or register number and 1 for the data. */
typedef void (*Y8960TunnelWrite)(void* ref, int port, UInt8 value);

/* A block registers itself when it is created and clears the entry when it
** is destroyed. Writes to an unclaimed tunnel go nowhere. */
void y8960RegisterTunnel(Y8960TunnelBlock block, Y8960TunnelWrite write, void* ref);
void y8960UnregisterTunnel(Y8960TunnelBlock block, void* ref);

/* One block per RomType, so a machine configuration can carry them
** independently while the cartridge hardware is still being designed.
**
** The Destroy entry points exist for the whole-card cartridge below. These
** blocks hold no slot, so ejecting the cartridge cannot reach them the way
** it reaches a mapper; the card has to take them down by name. They are not
** for shutdown: there the device manager destroys every block itself, and
** calling them as well would free each block twice. */
int  romMapperY8960OpllexCreate(void);
void romMapperY8960OpllexDestroy(void);
int  romMapperY8960Opl2exCreate(void);
void romMapperY8960Opl2exDestroy(void);
int  romMapperY8960SsgsCreate(void);
void romMapperY8960SsgsDestroy(void);
int  romMapperY8960TimerCreate(void);
void romMapperY8960TimerDestroy(void);
int  romMapperY8960MixerCreate(void);
void romMapperY8960MixerDestroy(void);
/* The SCC block carries the cartridge ROM and the bank mapper, so unlike the
** others it is created from the slot entry. */
int  romMapperY8960SccCreate(const char* filename, UInt8* romData,
                             int size, int slot, int sslot, int startPage);

/* The whole card: the SCC block plus the five other blocks, as a cartridge.
** romData may be NULL, which is the card with no firmware in it. */
int  romMapperY8960CartCreate(const char* filename, UInt8* romData,
                              int size, int slot, int sslot);

#endif
