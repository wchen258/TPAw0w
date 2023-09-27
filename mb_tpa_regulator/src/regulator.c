/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"
#include "etm_pkt_headers.h"

// MISC, PRINTF
void report(const char* format, ... ) {
	va_list args;
	xil_printf("REGULATOR: ");
	va_start(args, format);
	xil_vprintf(format, args);
	va_end(args);
	xil_printf("\n\r");
}

// All 4 buffers are 4K in total, each buffer is 1K
#define PTRC_BUFFER_SIZE 4096
volatile uint8_t ptrc_buf[4][PTRC_BUFFER_SIZE / 4] __attribute__((section(".ptrc_buf_zone")));
volatile uint32_t ptrc_abs_wpt[4] __attribute__((section(".ptrc_buf_pointer_zone")));
volatile uint32_t ptrc_abs_rpt[4];

volatile uint32_t debug_flag = 0;
uint8_t in_range[4];

uint32_t debug_count1[4];
uint32_t debug_count2[4];
uint32_t debug_count3[4];

static inline void debug_ptr(uint32_t point) {
	debug_flag = debug_flag | (0x1 << point);
}

uint8_t try_read_data(uint8_t* dest, uint32_t buffer_id, uint32_t bytes, uint32_t offset) {
	if (ptrc_abs_rpt[buffer_id] + (bytes + offset) > ptrc_abs_wpt[buffer_id])
		return 0;

	uint32_t read;
	uint32_t* buffer = ptrc_buf[buffer_id];

	for (read = 0; read < bytes; ++read) {
		dest[read] = buffer[(ptrc_abs_rpt[buffer_id] + offset + read) % (PTRC_BUFFER_SIZE / 4)];
	}

	return read;
}

void inc_buffer_rpt(uint32_t buffer_id, uint32_t func_result, uint8_t header_offset) {
	if (func_result > 0)
		ptrc_abs_rpt[buffer_id] = ptrc_abs_rpt[buffer_id] + func_result + header_offset;
}

uint32_t handle_exc_or_shortaddr(uint32_t buffer_id) {
	uint8_t payload;

	if (try_read_data(&payload, buffer_id, 1, 1) == 0)
		return 0;

	if ((payload >> 7) == 1) {
		if (try_read_data(&payload, buffer_id, 1, 2) == 0)
			return 0;

		return 2;
	}

	return 1;
}

uint32_t handle_longaddress(uint32_t buffer_id, uint8_t header) {
	uint8_t payload[8];
	uint64_t address;

	switch (header) {
		case LongAddress0:
			if (try_read_data(&payload, buffer_id, 8, 1) == 0)
				return 0;

			address = 0;
			address = address | (((uint64_t) payload[0] & 0x7f) << 2) |
				(((uint64_t)payload[1] & 0x7f) << 9) |
				(((uint64_t)payload[2]) << 16) | (((uint64_t)payload[3]) << 24) |
				(((uint64_t)payload[4]) << 32) | (((uint64_t)payload[5]) << 40) |
				(((uint64_t)payload[6]) << 48) | (((uint64_t)payload[7]) << 56);

			debug_count2[buffer_id]++;
			if (in_range[buffer_id]) {
				debug_count3[buffer_id]++;
				in_range[buffer_id] = 0;
			}

			return 8;
			break;
		case LongAddress1:
			if (try_read_data(&payload, buffer_id, 8, 1) == 0)
				return 0;

			debug_ptr(31);

			return 8;
			break;
		case LongAddress3:
		case LongAddress2:
			if (try_read_data(&payload, buffer_id, 4, 1) == 0)
				return 0;

			debug_ptr(30);

			return 4;
			break;
		default:
			debug_ptr(26);
			report("PAUSED, handle_longaddress default case");
			while(1);
	}
}

uint32_t handle_context(uint32_t buffer_id, uint8_t header, uint32_t handle_longaddr_offset) {
	uint8_t context_info, vmid;
	uint32_t contextid;

	uint32_t vmid_read = 0;
	uint32_t contextid_read = 0;

	if ((header & 0x1) == 0) {
		debug_ptr(29);
		report("PAUSED, handle_context (header & 0x1) == 0");
		while(1);
	} else {
		if (try_read_data(&context_info, buffer_id, 1, handle_longaddr_offset + 1) == 0)
			return 0;

		if (((context_info >> 6) & 1) == 1) {
			vmid_read = try_read_data(&vmid, buffer_id, 1, handle_longaddr_offset + 2);

			if (vmid_read == 0)
				return 0;

			debug_ptr(5);
		}

		if ((context_info >> 7) == 1) {
			contextid_read = try_read_data(&contextid, buffer_id, 4, handle_longaddr_offset + vmid_read + 2);

			if (contextid_read == 0)
				return 0;

			debug_ptr(6);
		}

		return 1 + vmid_read + contextid_read;
	}
}

uint32_t handle_addrwithcontext(uint32_t buffer_id, uint8_t header) {
	uint32_t handle_longaddress_offset, handle_context_offset;

	switch (header) {
		case AddrWithContext0:
			handle_longaddress_offset = handle_longaddress(buffer_id, LongAddress3);
			break;
		case AddrWithContext1:
			handle_longaddress_offset = handle_longaddress(buffer_id, LongAddress2);
			break;
		case AddrWithContext2:
			handle_longaddress_offset = handle_longaddress(buffer_id, LongAddress0);
			break;
		case AddrWithContext3:
			handle_longaddress_offset = handle_longaddress(buffer_id, LongAddress1);
			break;
		default:
			debug_ptr(25);
			report("PAUSED, handle_addrwithcontext default case");
			while(1);
	}

	if (handle_longaddress_offset == 0)
		return 0;

	handle_context_offset = handle_context(buffer_id, Context1, handle_longaddress_offset);

	if (handle_context_offset == 0)
		return 0;

	return handle_longaddress_offset + handle_context_offset;
}

void handle_buffer(uint32_t buffer_id) {
	uint8_t header;

	if (try_read_data(&header, buffer_id, 1, 0) == 0)
		return;

	switch (header) {
		case TraceOn:
			inc_buffer_rpt(buffer_id, 1, 0); // No function call, only account for header

			in_range[buffer_id] = 1;
			debug_count1[buffer_id]++;

			debug_ptr(0);

			break;

		case AddrWithContext0:
		case AddrWithContext1:
		case AddrWithContext2:
		case AddrWithContext3:
			inc_buffer_rpt(buffer_id, handle_addrwithcontext(buffer_id, header), 1);

			debug_ptr(1);

			break;

		case ShortAddr0:
		case ShortAddr1:
		case Exce:
			inc_buffer_rpt(buffer_id, handle_exc_or_shortaddr(buffer_id), 1);

			debug_ptr(2);

			break;

		case Async:
			inc_buffer_rpt(buffer_id, 11, 1);

			debug_ptr(3);

			break;
		case LongAddress0:// Long Address with 8B payload
		case LongAddress1:
			inc_buffer_rpt(buffer_id, 8, 1);

			debug_ptr(4);

			break;
		case LongAddress2:// Long Address with 4B payload
		case LongAddress3:
			inc_buffer_rpt(buffer_id, 4, 1);

			debug_ptr(5);

			break;
		case TraceInfo:
			inc_buffer_rpt(buffer_id, 2, 1);

			debug_ptr(6);

			break;

		case Atom10:
		case Atom11:
		case ExceReturn:
		case Event0:
		case Event1:
		case Event2:
		case Event3:
			inc_buffer_rpt(buffer_id, 1, 0); // No function call, only account for header

			debug_ptr(7);

			break;

		default:
			debug_ptr(21);
			report("PAUSED, handle_buffer default case, buffer_id: %d, header: 0x%x", buffer_id, header);
			while(1);
	}
}

void regulator_loop(void) {
	uint8_t i;

	while (1) {
		for (i = 0; i < 4; ++i) {
			handle_buffer(i);
		}
	}
}

void reset(void) {
	uint32_t i, j;

	for (i = 0; i < 4; ++i) {
		for (j = 0; j < PTRC_BUFFER_SIZE / 4; ++j)
			ptrc_buf[i][j] = 0;

		ptrc_abs_wpt[i] = 0;
		ptrc_abs_rpt[i] = 0;

		in_range[i] = 0;

		debug_count1[i] = 0;
		debug_count2[i] = 0;
		debug_count3[i] = 0;
	}

	debug_flag = 0;
}

int main()
{
    init_platform();

    sleep(2);
    report("Started (Not idle Regulator 1.0)");

    reset();

    report("Reset completed, starting loop");
    report("ptrc_buf address: 0x%x", ptrc_buf);
    report("ptrc_abs_wpt address: 0x%x", ptrc_abs_wpt);
    report("ptrc_abs_rpt address: 0x%x", ptrc_abs_rpt);
    report("debug_count1 address: 0x%x", debug_count1);
    report("debug_count2 address: 0x%x", debug_count2);
    report("debug_count3 address: 0x%x", debug_count3);
    report("debug_flag address: 0x%x", &debug_flag);

    debug_ptr(13);

    regulator_loop();

    cleanup_platform();
    return 0;
}
