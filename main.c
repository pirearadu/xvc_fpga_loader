/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd)
 * by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/).
 * "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 *  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <sys/mman.h>
#include <fcntl.h>
#include "xilinx_bit.h"

#define MAP_SIZE      0x10000
#ifdef __arm__
#define dsb(scope)    asm volatile("dsb " #scope : : : "memory")
#else
#define dsb(scope)
#endif

#define XCU_INSTR_LEN   (6)

typedef struct {
	uint32_t length_offset;
	uint32_t tms_offset;
	uint32_t tdi_offset;
	uint32_t tdo_offset;
	uint32_t ctrl_offset;
} jtag_t;

static int verbose = 0;

uint32_t jtag_shift_data(volatile jtag_t *ptr, uint32_t length, uint32_t tms, uint32_t tdi) {
	uint32_t tdo;

	//used to avoid bugs
	ptr->ctrl_offset = 0x00;
	dsb(st);
	ptr->length_offset = length;
	dsb(st);
	ptr->tms_offset = tms;
	dsb(st);
	ptr->tdi_offset = tdi;
	dsb(st);
	ptr->ctrl_offset = 0x01;
	/* Switch this to interrupt in next revision */
	while (ptr->ctrl_offset) {
		dsb(st);
	}

	tdo = ptr->tdo_offset;

	if (verbose) {
		printf("LEN : 0x%08x\t", length);
		printf("TMS : 0x%08x\t", tms);
		printf("TDI : 0x%08x\t", tdi);
		printf("TDO : 0x%08x\n", tdo);
	}
	return 0;
}

uint32_t jtag_exec_instruction(volatile jtag_t *ptr, uint32_t instr, uint32_t instr_len) {
	uint32_t tms = 1 << instr_len;

	jtag_shift_data(ptr, 2, 0b11, 0);
	jtag_shift_data(ptr, 2, 0, 0);
	jtag_shift_data(ptr, instr_len, tms, instr);
	jtag_shift_data(ptr, 1, 1, 0);

	return 0;
}

uint32_t jtag_write_bitstream(volatile jtag_t *ptr, struct xilinx_bit_file *bit_file) {
	uint32_t *bitstream_u32 = (uint32_t *) bit_file->data;
	uint32_t len_u32  = bit_file->length / 4;
	uint32_t last_word = len_u32 - 1;
	uint32_t percent = len_u32 / 100;
	uint32_t status = 0;
	uint32_t i;

	for (i = 0; i < bit_file->length; i++)
		bit_file->data[i] = flip_u32(bit_file->data[i], 8);

	jtag_shift_data(ptr, 1, 1, 0);
	jtag_shift_data(ptr, 2, 0, 0);

	for (i = 0; i < len_u32; i++) {
		if (i > (status * percent)) {
			printf("%d%%\n", status);
			status++;
		}

		if (i != last_word)
			jtag_shift_data(ptr, 32, 0, bitstream_u32[i]);
		else
			jtag_shift_data(ptr, 32, 1 << 31, bitstream_u32[i]);
	}

	jtag_shift_data(ptr, 1, 1, 0);

	return 0;
}

uint32_t jtag_rti_state(volatile jtag_t *ptr) {
	return jtag_shift_data(ptr, 2, 0b11, 0);
}

uint32_t jtag_tlr_state(volatile jtag_t *ptr) {
	return jtag_shift_data(ptr, 3, 0b111, 0);
}

uint32_t jtag_run_clk(volatile jtag_t *ptr, uint32_t clk_cycles) {
	uint32_t shift_num = clk_cycles >> 5;
	uint32_t rest = clk_cycles % 32;
	int i;

	for (i = 0; i < shift_num; i++)
		jtag_shift_data(ptr, 32, 0, 0);
	if (rest != 0)
		jtag_shift_data(ptr, rest, 0, 0);

	return 0;
}

int main(int argc, char **argv) {
	struct xilinx_bit_file bit_file;
	volatile jtag_t *ptr;
	char *file_path = NULL;
	int fd_uio;
	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "vf:b:")) != -1)
		switch (c) {
			case 'b':
			case 'f':
				file_path = malloc(strlen(optarg) + 1);
				strcpy(file_path, optarg);
				break;
			case 'v':
				verbose = 1;
				break;
			case '?':
				fprintf(stderr, "usage: %s [-v]\n", *argv);
				return 1;
		}

	if (!file_path) {
		fprintf(stderr, "ERROR: you must provide the bitsream.\n");
		return -1;
	}

	if (xilinx_read_bit_file(&bit_file, file_path)) {
		fprintf(stderr, "ERROR: fail to read the bitsream.\n");
		return -1;
	}

	fd_uio = open("/dev/uio0", O_RDWR);
	if (fd_uio < 1) {
		fprintf(stderr, "Failed to Open UIO Device\n");
		return -1;
	}

	ptr = (volatile jtag_t *) mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
				       fd_uio, 0);
	if (ptr == MAP_FAILED)
		fprintf(stderr, "MMAP Failed\n");

	close(fd_uio);
	verbose = 1;
	jtag_shift_data(ptr, 5, 0b11111, 0);
	printf("Reset done\n");
	jtag_rti_state(ptr);
	printf("Moved to rti done\n");
	jtag_exec_instruction(ptr, 0x0b, XCU_INSTR_LEN);
	printf("Executed 0xb\n");
	jtag_exec_instruction(ptr, 0x05, XCU_INSTR_LEN);
	printf("Executed 0x5\n");
	verbose = 0;
	jtag_run_clk(ptr, 20000);
	printf("Passed 20000 clock cycles\n");
	jtag_write_bitstream(ptr, &bit_file);
	printf("bitstream written \n");
	verbose = 1;
	jtag_rti_state(ptr);
	printf("Moved to rti done\n");
	jtag_exec_instruction(ptr, 0x0c, XCU_INSTR_LEN);
	printf("Executed 0xc\n");
	verbose = 0;
	jtag_run_clk(ptr, 2000);
	printf("Passed 2000 clock cycles\n");
	jtag_tlr_state(ptr);
	printf("Moved to tlr done\n");

	return 0;
}
