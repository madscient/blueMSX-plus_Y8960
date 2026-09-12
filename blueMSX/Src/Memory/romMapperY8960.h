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

/* One block per RomType, so a machine configuration can carry them
** independently while the cartridge hardware is still being designed. */
int romMapperY8960OpllexCreate(void);
int romMapperY8960Opl2exCreate(void);
int romMapperY8960SsgsCreate(void);
int romMapperY8960TimerCreate(void);
int romMapperY8960MixerCreate(void);
int romMapperY8960SccCreate(void);
int romMapperY8960DcsgCreate(void);

#endif
