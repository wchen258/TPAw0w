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
#include "tmg_monitor.h"

uint8_t hit_last = 0;

void regulator_loop(void) {
	uint8_t i;

	while (1) {
		for (i = 0; i < 4; ++i) {
			if (((hit_last >> i) & 0b1) == 0) {
				handle_buffer(i);
				handle_hit(i);
			}
		}
	}
}

void reset(void) {
	uint8_t i;

	for (i = 0; i < 4; ++i) {
		reset_ptrc_buf(i);
	}

	reset_tmg_buf();
}

int main() {
    init_platform();

    sleep(2);
    report("Started (Regulator 1.00008.2)");
    report("dbg          address: 0x%x", &dbg);

    reset();

	report_ptrc_mem();
	report_tmg_mem();

    regulator_loop();

    cleanup_platform();
    return 0;
}
