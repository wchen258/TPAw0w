#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "sleep.h"
#include "etm_pkt_headers.h"
#include "timed_milestone_graph.h"
#include "dbg_util.h"
#include "log_util.h"
#include "xil_cache.h"
//#include "xil_cache_l.h"
#include "ptrc_parser.h"
#include "common.h"
#include "tmg_monitor.h"

uint8_t hit_last = 0;

static void initial_hotreset(void) {
    report("HOTRESET Initiated");

    while (inter_rpu_com->hotreset == 1);

    report("DEF Should have completed reset, continue");
}

void regulator_loop(void) {
	uint8_t i;

	while (1) {
        if (inter_rpu_com->hotreset) {
            initial_hotreset();
            break;
        }

		for (i = 0; i < 4; ++i) {
			if (((hit_last >> i) & 0b1) == 0) {
				handle_buffer(i);
				handle_hit(i);
			}
		}
	}
}

void reset(void) {
	reset_ptrc();
    reset_dbg_util();
    reset_log_util();

    hit_last = 0;

    reset_tmg();
}

int main() {
    init_platform();

    //Xil_DCacheFlush();
    //Xil_DCacheDisable();
    //Xil_ICacheDisable();
    //Xil_L1DCacheDisable();
    //Xil_L1ICacheDisable();
    //Xil_L2CacheDisable();

    sleep(2);
    report("Started (Regulator 1.00016)");
    report("dbg address: 0x%x", &dbg);

    //breport("TestT");
    // breport("T%d %x", 123, 0xdeadbeef);
    // report("breport testing finished");

    report("pointer: 0x%x", &inter_rpu_com->report_lock);

    while(1) {
        reset();
	// report_ptrc_mem();
	// report_tmg_mem();
        regulator_loop();
        report("loop exits");
    }

    cleanup_platform();
    return 0;
}
