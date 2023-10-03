#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"
#include "etm_pkt_headers.h"
#include "timed_milestone_graph.h"
#include "dbg_util.h"
#include "xil_cache.h"
#include "ptrc_parser.h"
#include "common.h"


volatile uint8_t tmg_buf[4][TMG_BUFFER_SIZE / 4] __attribute__((section(".tmg_mem_zone")));
tmg_node* tmg_current_node[4];

void regulator_loop(void) {
	uint8_t i;

	while (1) {
		for (i = 0; i < 4; ++i) {
			// virtual_offset = 0;
			// virtual_read_failed = 0;
			// current_buffer_id = i;

			handle_buffer(i);

			// if (virtual_read_failed == 0) {
			// 	apply_virtual_offset(); 
			// }
		}
	}
}

void reset(void) {
	uint32_t i, j;

	for (i = 0; i < 4; ++i) {
		// for (j = 0; j < PTRC_BUFFER_SIZE / 4; ++j)
		// 	ptrc_buf[i][j] = 0;

		reset_ptrc_buf(i);

		for (j = 0; j < TMG_BUFFER_SIZE / 4; ++j)
			tmg_buf[i][j] = 0xFF;

		dbg.tmg_ready = 0;

		// ptrc_abs_wpt[i] = 0;
		// ptrc_abs_rpt[i] = 0;

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
    report("Started (Regulator 1.00003)");

    reset();

    report("Reset completed. Ready to load TMG buffer");

    // process_tmg_buffer();

    report("TMG Buffer Loaded. Starting loop");
	report_ptrc_mem();
    report("dbg          address: 0x%x", &dbg);
    report("debug_count1 address: 0x%x", debug_count1);
    report("debug_count2 address: 0x%x", debug_count2);
    report("debug_count3 address: 0x%x", debug_count3);
    report("debug_flag   address: 0x%x", &debug_flag);

    debug_ptr(13);

    regulator_loop();

    cleanup_platform();
    return 0;
}
