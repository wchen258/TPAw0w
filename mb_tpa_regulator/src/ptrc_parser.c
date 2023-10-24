#include <stdint.h>
#include "ptrc_parser.h"
#include "etm_pkt_headers.h"
#include "dbg_util.h"
#include "xtime_l.h"

#include "tmg_monitor.h"

// All 4 buffers are 4K in total, each buffer is 1K
static volatile uint8_t ptrc_buf[4][PTRC_BUFFER_SIZE / 4] __attribute__((section(".ptrc_buf_zone")));
static volatile uint32_t ptrc_abs_wpt[4] __attribute__((section(".ptrc_buf_pointer_zone")));
static volatile uint32_t ptrc_abs_rpt[4] __attribute__((section(".ptrc_buf_pointer_zone")));
static uint32_t virtual_offset = 0;
static uint8_t virtual_read_failed = 0;
static uint32_t current_buffer_id = 0;
static uint8_t in_range[4];

static uint32_t try_read_data(uint8_t* dest, uint32_t bytes) {
	if (virtual_read_failed) {
		report("PAUSED, repeated call to try_read_data when virtual_read_failed");
		SET(dbg.fault, 0);
		while(1);
	}

	if (ptrc_abs_rpt[current_buffer_id] + (bytes + virtual_offset) > ptrc_abs_wpt[current_buffer_id]) {
		virtual_read_failed = 1;
		return 0;
	}


	/*
	uint32_t write_pointer = ptrc_abs_wpt[current_buffer_id];
	while (ptrc_abs_rpt[current_buffer_id] + (bytes + virtual_offset) > write_pointer) {
		dbg.vals[3]++;
		write_pointer = ptrc_abs_wpt[current_buffer_id];
	}
	*/


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

			if (in_range[current_buffer_id]) {
				report_address_hit(current_buffer_id, address);
				dbg.on_ct += 1;
				in_range[current_buffer_id] = 0;
			}

			break;
		case LongAddress1:
			if (try_read_data(&payload[0], 8) == 0)
				return;

			break;
		case LongAddress3:
		case LongAddress2:
			if (try_read_data(&payload[0], 4) == 0)
				return;

			break;
		default:
			report("PAUSED, handle_longaddress default case");
			SET(dbg.fault, 1);
			while(1);
	}
}

static void handle_context(uint8_t header) {
	uint8_t context_info, vmid;
	uint8_t contextid[4];

	if ((header & 0x1) == 0) {
		report("PAUSED, handle_context (header & 0x1) == 0");
		SET(dbg.fault, 2);
		while(1);
	} else {
		if (try_read_data(&context_info, 1) == 0)
			return;

		if (((context_info >> 6) & 1) == 1) {
			if (try_read_data(&vmid, 1) == 0)
				return;
		}

		if ((context_info >> 7) == 1) {
			if (try_read_data(&contextid[0], 4) == 0)
				return;
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
			report("PAUSED, handle_addrwithcontext default case");
			SET(dbg.fault, 3);
			while(1);
	}

	if (virtual_read_failed)
		return;

	handle_context(Context1);
}

void handle_buffer(uint8_t buffer_id) {
    virtual_offset = 0;
    virtual_read_failed = 0;
    current_buffer_id = buffer_id;

	uint8_t header;

	if (try_read_data(&header, 1) == 0) {
		return;
    } else {
	    switch (header) {
		    case TraceOn:
			    in_range[current_buffer_id] = 1;

			    /*if (dbg.vals[0] < 8) {
			    	XTime trace_on_time;
			    	XTime_GetTime(&trace_on_time);
			    	dbg.trace_on_timings[dbg.vals[0]] = (trace_on_time / COUNTS_PER_USECOND);
			    }

			    if (dbg.vals[0] < 4)
			    	dbg.traceon_frames[dbg.vals[0]] = ptrc_buf[1][(ptrc_abs_rpt[current_buffer_id]) % (PTRC_BUFFER_SIZE / 4)];
			    */
			    dbg.vals[0] += 1;
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
			    dbg.trace_on_timings[1] = 0xF00C;
			    dbg.trace_on_timings[2]++;
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
		    	SET(dbg.fault, 4);
		    	dbg.trace_on_timings[0] = header;
			    report("PAUSED, handle_buffer default case, buffer_id: %d, header: 0x%x", current_buffer_id, header);
			    while(1);
	    }
    }

    if (virtual_read_failed == 0) {
        apply_virtual_offset();
    }
}

void reset_ptrc_buf(uint8_t buffer_id) {
    int j;
	for (j = 0; j < PTRC_BUFFER_SIZE / 4; ++j) {
		ptrc_buf[buffer_id][j] = 0;
	}
    ptrc_abs_rpt[buffer_id] = 0;
    ptrc_abs_wpt[buffer_id] = 0;
    in_range[buffer_id] = 0;
}

void report_ptrc_mem() {
    report("PTRC MEMO GROUP START (no-speculation, traceon framed)");
    report("ptrc_buf     address: 0x%x", ptrc_buf);
    report("ptrc_abs_wpt address: 0x%x", ptrc_abs_wpt);
    report("ptrc_abs_rpt address: 0x%x", ptrc_abs_rpt);
    report("virtual offset      : 0x%x", &virtual_offset);
    report("virtual_read_failed : 0x%x", &virtual_read_failed);
    report("PTRC MEMO GROUP END");
}
