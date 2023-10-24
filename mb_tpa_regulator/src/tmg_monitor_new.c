/*

#include <stdint.h>
#include "coresight_lib.h"
#include "dbg_util.h"
#include "xtime_l.h"
#include "tmg_monitor_old.h"

typedef struct milestone {
    uint32_t address;
    uint32_t tail_time;
    uint8_t child_offset[4];
    uint32_t child_nominal_time[4];
} __attribute__((__packed__)) milestone;

static milestone * ms_under_monitor[4];

static volatile uint8_t tmg_buf[4][TMG_BUFFER_SIZE / 4] __attribute__((section(".tmg_mem_zone")));

static uint32_t reported_hit[4];

void report_address_hit(uint8_t id, uint32_t address) {
    reported_hit[id] = address;
}

static void set_new_ms_under_monitor(uint8_t id, uint32_t nominal_time, milestone * new_ms) {
	etm_disable(id);

	for (j = 0; j < 4; ++j) {
		if (new_ms->child_offset[j] == 0xFF) {
			for (; j < 4; ++j) {
				etm_write_acvr_pair(id, j, 0);
			}
		} else {
			etm_write_acvr_pair(id, j, ((milestone*) tmg_buf[id])[new_ms->child_offset[j]].address);
		}
	}

	etm_enable(id);
}

void handle_hit(uint8_t id) {
    if (reported_hit[id] == 0 || reported_hit[id] == ms_under_monitor[id]->address)
        return;

    uint32_t j = 0;

    for (j = 0; j < 4 && ms_under_monitor[id]->child_offset[j] != 0xFF; ++j) {
    	milestone * child = ((milestone*) tmg_buf[id])[ms_under_monitor[id]->child_offset[j]];

    	if (child->address == reported_hit[id]) {
    		dbg.traceon_frames[0]++;

    		set_new_ms_under_monitor(id, ms_under_monitor[id]->child_nominal_time[j], child);
    		break;
    	}
    }
}

void reset_tmg_buf() {
    int i, j;

    for (i = 0; i < 4; ++i) {
		for (j = 0; j < TMG_BUFFER_SIZE / 4; ++j)
			tmg_buf[i][j] = 0xFF;

		reported_hit[i] = 0;
    }

    dbg.tmg_ready = 0;

    report("tmg_buf reset, waiting for tmg graph");

    while(dbg.tmg_ready == 0);

    for (i = 0; i < 4; ++i)
    	ms_under_monitor[i] = (milestone *) &tmg_buf[i][0];
}

void report_tmg_mem() {
    report("TMG MEMO GROUP START (2.7)");
    report("tmg_buf      address: 0x%x", tmg_buf);

    uint32_t i, j;
    for (i = 0; i < 4; ++i) {
        report("ms_under_monitor[%d].address    0x%x", i, ms_under_monitor[i]->address);
        report("ms_under_monitor[%d].tail_time    0x%x", i, ms_under_monitor[i]->tail_time);

        for (j = 0; j < 4; ++j) {
            report("ms_under_monitor[%d].ch_offset[%d]    0x%x", i, j, ms_under_monitor[i]->children[j].offset);
            report("ms_under_monitor[%d].nol_times[%d]    0x%x", i, j, ms_under_monitor[i]->children[j].nominal_time);
        }
    }

    report("TMG MEMO GROUP END");
}

*/
