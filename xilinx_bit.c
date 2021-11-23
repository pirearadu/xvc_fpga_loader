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

#include "xilinx_bit.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define LOG_ERROR(format, ...) fprintf(stderr, format "\n" __VA_OPT__(,) __VA_ARGS__)
#define LOG_INFO(format, ...) fprintf(stdout, format "\n" __VA_OPT__(,) __VA_ARGS__)
#define PRIu32  "%u"

#define ERROR_OK                    (0)
#define ERROR_PLD_FILE_LOAD_FAILED  (-1)
#define ERROR_COMMAND_SYNTAX_ERROR  (-2)

static const unsigned char bit_reverse_table256[] = {
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};


uint32_t flip_u32(uint32_t value, unsigned int num)
{
    uint32_t c = (bit_reverse_table256[value & 0xff] << 24) |
                 (bit_reverse_table256[(value >> 8) & 0xff] << 16) |
                 (bit_reverse_table256[(value >> 16) & 0xff] << 8) |
                 (bit_reverse_table256[(value >> 24) & 0xff]);

    if (num < 32)
        c = c >> (32 - num);

    return c;
}

static int read_section(FILE *input_file, int length_size, char section,
	uint32_t *buffer_length, uint8_t **buffer)
{
	uint8_t length_buffer[4];
	int length;
	char section_char;
	int read_count;

	if ((length_size != 2) && (length_size != 4)) {
		LOG_ERROR("BUG: length_size neither 2 nor 4");
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	read_count = fread(&section_char, 1, 1, input_file);
	if (read_count != 1)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (section_char != section)
		return ERROR_PLD_FILE_LOAD_FAILED;

	read_count = fread(length_buffer, 1, length_size, input_file);
	if (read_count != length_size)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (length_size == 4)
		length = be_to_h_u32(length_buffer);
	else	/* (length_size == 2) */
		length = be_to_h_u16(length_buffer);

	if (buffer_length)
		*buffer_length = length;

	*buffer = malloc(length);

	read_count = fread(*buffer, 1, length, input_file);
	if (read_count != length)
		return ERROR_PLD_FILE_LOAD_FAILED;

	return ERROR_OK;
}

int xilinx_read_bit_file(struct xilinx_bit_file *bit_file, const char *filename)
{
	FILE *input_file;
	struct stat input_stat;
	int read_count;

	if (!filename || !bit_file)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (stat(filename, &input_stat) == -1) {
		LOG_ERROR("couldn't stat() %s: %s", filename, strerror(errno));
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	if (S_ISDIR(input_stat.st_mode)) {
		LOG_ERROR("%s is a directory", filename);
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	if (input_stat.st_size == 0) {
		LOG_ERROR("Empty file %s", filename);
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	input_file = fopen(filename, "rb");
	if (!input_file) {
		LOG_ERROR("couldn't open %s: %s", filename, strerror(errno));
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	read_count = fread(bit_file->unknown_header, 1, 13, input_file);
	if (read_count != 13) {
		LOG_ERROR("couldn't read unknown_header from file '%s'", filename);
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	if (read_section(input_file, 2, 'a', NULL, &bit_file->source_file) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 2, 'b', NULL, &bit_file->part_name) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 2, 'c', NULL, &bit_file->date) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 2, 'd', NULL, &bit_file->time) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 4, 'e', &bit_file->length, &bit_file->data) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	LOG_INFO("bit_file: %s %s %s,%s %" PRIu32 "", bit_file->source_file, bit_file->part_name,
		bit_file->date, bit_file->time, bit_file->length);
    LOG_INFO("\n\n\n\n\n\n");
	fclose(input_file);

	return ERROR_OK;
}
