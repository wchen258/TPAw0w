#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"
#include "etm_pkt_headers.h"
#include "timed_milestone_graph.h"
#include "dbg_util.h"
#include "xil_cache.h"

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
volatile uint32_t ptrc_abs_rpt[4] __attribute__((section(".ptrc_buf_pointer_zone")));

#define TMG_BUFFER_SIZE 4096
volatile uint8_t tmg_buf[4][TMG_BUFFER_SIZE / 4] __attribute__((section(".tmg_mem_zone")));
tmg_node* tmg_current_node[4];

uint32_t virtual_offset = 0;
uint8_t virtual_read_failed = 0;
uint32_t current_buffer_id = 0;

volatile uint32_t debug_flag = 0;
uint8_t in_range[4];
uint32_t debug_count1[4];
uint32_t debug_count2[4];
uint32_t debug_count3[4];

static inline void debug_ptr(uint32_t point) {
	debug_flag = debug_flag | (0x1 << point);
}

volatile struct debugger __attribute__((section(".dbg_mem_zone"))) dbg = {0};

uint32_t try_read_data(uint8_t* dest, uint32_t bytes) {
	if (virtual_read_failed) {
		report("PAUSED, repeated call to try_read_data when virtual_read_failed");
		while(1);
	}

	if (ptrc_abs_rpt[current_buffer_id] + (bytes + virtual_offset) > ptrc_abs_wpt[current_buffer_id]) {
		virtual_read_failed = 1;
		return 0;
	}

	uint32_t read;

	for (read = 0; read < bytes; ++read) {
		dest[read] = ptrc_buf[current_buffer_id][(ptrc_abs_rpt[current_buffer_id] + virtual_offset + read) % (PTRC_BUFFER_SIZE / 4)];
	}

	virtual_offset = virtual_offset + read;

	return read;
}

void apply_virtual_offset() {
	ptrc_abs_rpt[current_buffer_id] = ptrc_abs_rpt[current_buffer_id] + virtual_offset;
}

void handle_exc_or_shortaddr() {
	uint8_t payload;

	if (try_read_data(&payload, 1) == 0)
		return;

	if ((payload >> 7) == 1) {
		if (try_read_data(&payload, 1) == 0)
			return;
	}
}

void handle_longaddress(uint8_t header) {
	uint8_t payload[8];
	uint64_t address;

	switch (header) {
		case LongAddress0:
			if (try_read_data(&payload[0], 8) == 0)
				return;

			address = 0;
			address = address | (((uint64_t) payload[0] & 0x7f) << 2) |
				(((uint64_t)payload[1] & 0x7f) << 9) |
				(((uint64_t)payload[2]) << 16) | (((uint64_t)payload[3]) << 24) |
				(((uint64_t)payload[4]) << 32) | (((uint64_t)payload[5]) << 40) |
				(((uint64_t)payload[6]) << 48) | (((uint64_t)payload[7]) << 56);

			debug_count2[current_buffer_id]++;
			if (in_range[current_buffer_id]) {
				debug_count3[current_buffer_id]++;
				in_range[current_buffer_id] = 0;
			}

			break;
		case LongAddress1:
			if (try_read_data(&payload[0], 8) == 0)
				return;

			debug_ptr(31);

			break;
		case LongAddress3:
		case LongAddress2:
			if (try_read_data(&payload[0], 4) == 0)
				return;

			debug_ptr(30);

			break;
		default:
			debug_ptr(26);
			report("PAUSED, handle_longaddress default case");
			while(1);
	}
}

void handle_context(uint8_t header) {
	uint8_t context_info, vmid;
	uint8_t contextid[4];

	if ((header & 0x1) == 0) {
		debug_ptr(29);
		report("PAUSED, handle_context (header & 0x1) == 0");
		while(1);
	} else {
		if (try_read_data(&context_info, 1) == 0)
			return;

		if (((context_info >> 6) & 1) == 1) {
			if (try_read_data(&vmid, 1) == 0)
				return;

			debug_ptr(5);
		}

		if ((context_info >> 7) == 1) {
			if (try_read_data(&contextid[0], 4) == 0)
				return;

			debug_ptr(6);
		}
	}
}

void handle_addrwithcontext(uint8_t header) {
	switch (header) {
		case AddrWithContext0:
			handle_longaddress(LongAddress3);
			break;
		case AddrWithContext1:
			handle_longaddress(LongAddress2);
			break;
		case AddrWithContext2:
			handle_longaddress(LongAddress0);
			break;
		case AddrWithContext3:
			handle_longaddress(LongAddress1);
			break;
		default:
			debug_ptr(25);
			report("PAUSED, handle_addrwithcontext default case");
			while(1);
	}

	if (virtual_read_failed)
		return;

	handle_context(Context1);
}

void handle_buffer(void) {
	uint8_t header;

	if (try_read_data(&header, 1) == 0)
		return;

	switch (header) {
		case TraceOn:
			in_range[current_buffer_id] = 1;
			debug_count1[current_buffer_id]++;

			debug_ptr(0);
			dbg.select = 1 << 0;

			break;

		case AddrWithContext0:
		case AddrWithContext1:
		case AddrWithContext2:
		case AddrWithContext3:
			handle_addrwithcontext(header);

			debug_ptr(1);

			dbg.select = 1 << 1;
			break;

		case ShortAddr0:
		case ShortAddr1:
		case Exce:
			handle_exc_or_shortaddr();

			debug_ptr(2);

			dbg.select = 1 << 2;

			break;

		case Async:
			virtual_offset += 11;

			debug_ptr(3);

			dbg.select = 1 << 3;
			break;
		case LongAddress0:// Long Address with 8B payload
		case LongAddress1:
			virtual_offset += 8;

			debug_ptr(4);

			dbg.select = 1 << 4;
			break;
		case LongAddress2:// Long Address with 4B payload
		case LongAddress3:
			virtual_offset += 4;

			debug_ptr(5);
			dbg.select = 1 << 5;
			break;
		case TraceInfo:
			virtual_offset += 2;

			debug_ptr(6);
			dbg.select = 1 << 6;
			break;

		case Atom10:
		case Atom11:
		case ExceReturn:
		case Event0:
		case Event1:
		case Event2:
		case Event3:
			debug_ptr(7);
			dbg.select = 1 << 7;
			break;

		default:
			dbg.vals[0] = ptrc_abs_wpt[0];
			dbg.vals[1] = ptrc_abs_rpt[0];
			Xil_DCacheFlush(); // let me try to flush the whole thing. 
			debug_ptr(21);
			report("PAUSED, handle_buffer default case, buffer_id: %d, header: 0x%x", current_buffer_id, header);
			while(1);
	}
}

void regulator_loop(void) {
	uint8_t i;

	while (1) {
		for (i = 0; i < 4; ++i) {
			virtual_offset = 0;
			virtual_read_failed = 0;
			current_buffer_id = i;

			handle_buffer();

			if (virtual_read_failed == 0) {
				apply_virtual_offset(); // TODO: probably this is why the r/w pts have gap btwn
			}
		}
	}
}

void reset(void) {
	uint32_t i, j;

	for (i = 0; i < 4; ++i) {
		for (j = 0; j < PTRC_BUFFER_SIZE / 4; ++j)
			ptrc_buf[i][j] = 0;

		for (j = 0; j < TMG_BUFFER_SIZE / 4; ++j)
			tmg_buf[i][j] = 0xFF;

		dbg.tmg_ready = 0;

		ptrc_abs_wpt[i] = 0;
		ptrc_abs_rpt[i] = 0;

		in_range[i] = 0;

		debug_count1[i] = 0;
		debug_count2[i] = 0;
		debug_count3[i] = 0;
	}

	debug_flag = 0;
}

void process_tmg_buffer(void) {
	while (dbg.tmg_ready == 0);

	int i = 0, j;

	for (i = 0; i < 4; ++i) {
		uint32_t * buffer = (uint32_t*) &tmg_buf[i][0];
		j = 2;

		while (j < TMG_BUFFER_SIZE / 16) {
			buffer[j] = buffer[j] + (uint32_t) &buffer[0];

			j += 2;

			if (buffer[j] == 0xFFFFFFFF) {
				j += 3;

				if (j >= TMG_BUFFER_SIZE / 16 || buffer[j] == 0xFFFFFFFF)
					break;
			}
		}
	}
}

int main() {
    init_platform();

    sleep(2);
    report("Started (Regulator 1.00002)");

    reset();

    report("Reset completed. Ready to load TMG buffer");

    process_tmg_buffer();

    report("TMG Buffer Loaded. Starting loop");
    report("ptrc_buf     address: 0x%x", ptrc_buf);
    report("ptrc_abs_wpt address: 0x%x", ptrc_abs_wpt);
    report("ptrc_abs_rpt address: 0x%x", ptrc_abs_rpt);
    report("dbg          address: 0x%x", &dbg);
    report("debug_count1 address: 0x%x", debug_count1);
    report("debug_count2 address: 0x%x", debug_count2);
    report("debug_count3 address: 0x%x", debug_count3);
    report("debug_flag   address: 0x%x", &debug_flag);
    report("virtual offset      : 0x%x", &virtual_offset);
    report("virtual_read_failed : 0x%x", &virtual_read_failed);

    debug_ptr(13);

    regulator_loop();

    cleanup_platform();
    return 0;
}
