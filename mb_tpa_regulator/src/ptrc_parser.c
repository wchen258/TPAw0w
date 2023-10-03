#include <stdint.h>
#include "ptrc_parser.h"
#include "etm_pkt_headers.h"
#include "dbg_util.h"

// All 4 buffers are 4K in total, each buffer is 1K
volatile uint8_t ptrc_buf[4][PTRC_BUFFER_SIZE / 4] __attribute__((section(".ptrc_buf_zone")));
volatile uint32_t ptrc_abs_wpt[4] __attribute__((section(".ptrc_buf_pointer_zone")));
volatile uint32_t ptrc_abs_rpt[4] __attribute__((section(".ptrc_buf_pointer_zone")));
uint32_t virtual_offset = 0;
uint8_t virtual_read_failed = 0;
uint32_t current_buffer_id = 0;

static uint32_t try_read_data(uint8_t* dest, uint32_t bytes) {
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

static void handle_exc_or_shortaddr() {
	uint8_t payload;

	if (try_read_data(&payload, 1) == 0)
		return;

	if ((payload >> 7) == 1) {
		if (try_read_data(&payload, 1) == 0)
			return;
	}
}

static void handle_longaddress(uint8_t header) {
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

static void handle_context(uint8_t header) {
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

static void handle_addrwithcontext(uint8_t header) {
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
			dbg.select = 1 << 0;
			break;

		case AddrWithContext0:
		case AddrWithContext1:
		case AddrWithContext2:
		case AddrWithContext3:
			handle_addrwithcontext(header);
			dbg.select = 1 << 1;
			break;

		case ShortAddr0:
		case ShortAddr1:
		case Exce:
			handle_exc_or_shortaddr();
			dbg.select = 1 << 2;
			break;

		case Async:
			virtual_offset += 11;
			dbg.select = 1 << 3;
			break;
		case LongAddress0:// Long Address with 8B payload
		case LongAddress1:
			virtual_offset += 8;
			dbg.select = 1 << 4;
			break;
		case LongAddress2:// Long Address with 4B payload
		case LongAddress3:
			virtual_offset += 4;
			dbg.select = 1 << 5;
			break;
		case TraceInfo:
			virtual_offset += 2;
			dbg.select = 1 << 6;
			break;

		case Atom10:
		case Atom11:
		case ExceReturn:
		case Event0:
		case Event1:
		case Event2:
		case Event3:
			dbg.select = 1 << 7;
			break;

		default:
			debug_ptr(21);
			report("PAUSED, handle_buffer default case, buffer_id: %d, header: 0x%x", current_buffer_id, header);
			while(1);
	}
}