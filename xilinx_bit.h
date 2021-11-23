/***************************************************************************
 *   Copyright (C) 2006 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef OPENOCD_PLD_XILINX_BIT_H
#define OPENOCD_PLD_XILINX_BIT_H

#include <stdint.h>

struct xilinx_bit_file {
	uint8_t unknown_header[13];
	uint8_t *source_file;
	uint8_t *part_name;
	uint8_t *date;
	uint8_t *time;
	uint32_t length;
	uint8_t *data;
};

uint32_t flip_u32(uint32_t value, unsigned int num);

int xilinx_read_bit_file(struct xilinx_bit_file *bit_file, const char *filename);

static inline uint32_t be_to_h_u32(const uint8_t *buf)
{
    return (uint32_t)((uint32_t)buf[3] | (uint32_t)buf[2] << 8 | (uint32_t)buf[1] << 16 | (uint32_t)buf[0] << 24);
}

static inline uint16_t be_to_h_u16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[1] | (uint16_t)buf[0] << 8);
}

#endif /* OPENOCD_PLD_XILINX_BIT_H */
