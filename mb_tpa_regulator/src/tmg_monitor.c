#include <stdint.h>
#include "tmg_monitor.h"
#include "coresight_lib.h"
#include "dbg_util.h"
#include "xtime_l.h"

static volatile uint8_t tmg_buf[4][TMG_BUFFER_SIZE / 4] __attribute__((section(".tmg_mem_zone")));

static uint32_t reported_hit[4];

typedef struct {
	uint32_t offset;
	uint32_t nominal_time;
} __attribute__((packed)) ms_child;

typedef struct {
    uint32_t address;
    uint32_t tail_time;
    ms_child children[4];
} __attribute__((packed)) milestone;

static milestone * ms_under_monitor[4];
static milestone ms_fake_parent[4];

void report_address_hit(uint8_t id, uint32_t address) {
    reported_hit[id] = address;
}

static void set_new_ms_under_monitor(uint8_t id, uint32_t nominal_time, milestone * new_ms) {
    //TODO: handle regulation logic
    
    uint8_t j, reached_last_child = 0;
    etm_disable(id);

    for (j = 0; j < 4; ++j) {
        if (new_ms->children[j].offset == 0xFFFFFFFF) {
            reached_last_child = 1;
        }

        if (reached_last_child) {
        	dbg.assert = dbg.assert | 0b100;
        	etm_write_acvr_pair(id, j, 0);

        	dbg.current_monitoring[j] = 0;
        } else {
        	dbg.assert = dbg.assert | 0b1000;
        	etm_write_acvr_pair(id, j, ((milestone *) &tmg_buf[id][new_ms->children[j].offset])->address);

        	dbg.current_monitoring[j] = ((milestone *) &tmg_buf[id][new_ms->children[j].offset])->address;
        }
    }

    dbg.ms_updates[id]++;

    etm_enable(id);

    dbg.assert = dbg.assert | 0b10000;

    ms_under_monitor[id] = new_ms;
}

void handle_hit(uint8_t id) {
    if (reported_hit[id] == 0 || reported_hit[id] == ms_under_monitor[id]->address)
        return;

    uint32_t j = 0;

    for (j = 0; j < 4 && ms_under_monitor[id]->children[j].offset != 0xFFFFFFFF; ++j) {
        milestone * child = (milestone *) &tmg_buf[id][ms_under_monitor[id]->children[j].offset];

        dbg.assert = dbg.assert | 0b1;

        //report("%d, 0%x == 0%x\n\r", id, child->address, reported_hit[id]);

        if (dbg.vals[1] == 0) {
			dbg.vals[1] = id + 1;
			dbg.vals[2] = child->address;
			dbg.vals[3] = reported_hit[id];

			if (reported_hit[id] == 0) {
				dbg.fault = 0xFF23;
			}
		}

        if (child->address == reported_hit[id]) {
        	dbg.traceon_frames[0]++;



            set_new_ms_under_monitor(id, ms_under_monitor[id]->children[j].nominal_time, child);
            break;
        }
    }

    reported_hit[id] = 0;
}

void reset_tmg_buf() {
    int i, j;

    for (i = 0; i < 4; ++i) {
		for (j = 0; j < TMG_BUFFER_SIZE / 4; ++j)
			tmg_buf[i][j] = 0xFF;

		reported_hit[i] = 0;

		//milestone * first_ms = (milestone *) &tmg_buf[i][0];
		//milestone fake_parent = {};
		//fake_parent.children[0].offset = 0;
		//fake_parent.children[0].nominal_time = 0;
		//fake_parent.children[1].offset = 0xFFFFFFFF;

		//ms_under_monitor[i] = (milestone *) &tmg_buf[i][0];

		ms_fake_parent[i].children[0].offset = 0;
		ms_fake_parent[i].children[0].nominal_time = 0;
		ms_fake_parent[i].children[1].offset = 0xFFFFFFFF;
		ms_under_monitor[i] = &ms_fake_parent[i];
    }

    dbg.tmg_ready = 0;

    report("tmg_buf reset, waiting for tmg graph");

    while(dbg.tmg_ready == 0);
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
