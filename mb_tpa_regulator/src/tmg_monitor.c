#include <stdint.h>
#include "tmg_monitor.h"
#include "dbg_util.h"
#include "log_util.h"
#include "xtime_l.h"
#include "coresight_lib.h"
#include "tpa_scheduler.h"

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
static uint32_t event_address_map[4][4];
static uint32_t tmp_cti_test = 0;

static XTime real_time_base [4] = {0};
static uint8_t rt_base_log_pt [4] = {190, 190, 190, 190};
static uint32_t acc_nominal_time [4] = {0};
uint32_t entry_addr [4] = {0};
extern uint8_t hit_last;

void report_event_hit(uint8_t id, uint8_t event) {
	reported_hit[id] = event_address_map[id][event];
}

void report_address_hit(uint8_t id, uint32_t address) {
    reported_hit[id] = address;
}

static void set_new_ms_under_monitor(uint8_t id, uint32_t current_ms_address, uint32_t nominal_time, milestone * new_ms) {
    uint8_t j, reached_last_child = 0;
    struct ms_record log_ms_record = {0};

    log_ms_record.address_parent = current_ms_address;
    log_ms_record.address_child = new_ms->address;

    // calculate offset the starting time, handle timing issue if the task is periodic
    uint32_t real_time = 0;
    XTime timestamp;
    if (new_ms->address == entry_addr[id]) {
        XTime_GetTime(&real_time_base[id]);
        dbg.milestone_timestamps[id][rt_base_log_pt[id]++] = real_time_base[id];
        acc_nominal_time[id] = 0;
    } else {
        XTime_GetTime(&timestamp);
        real_time = (uint32_t) (timestamp - real_time_base[id]);
        acc_nominal_time[id] += nominal_time;
    }

    // logging the timing information
    uint8_t tpa_mode = dbg.tpa_mode & 0b111;
    if (tpa_mode == 0) {
        dbg.milestone_timestamps[id][dbg.ms_ts_pt[id]++] = real_time; 
        log_ms_record.timestamp = real_time;
    } else if (tpa_mode == 1) {
        dbg.milestone_timestamps[id][dbg.ms_ts_pt[id]++] = acc_nominal_time[id]; 
        log_ms_record.timestamp = acc_nominal_time[id];
    } else if (tpa_mode == 2) {
        dbg.milestone_timestamps[id][dbg.ms_ts_pt[id]++] = new_ms->tail_time; 
        log_ms_record.timestamp = new_ms->tail_time;
    }

    // invoke the scheduler
    // uint8_t do_regulation = 1; //(dbg.tpa_mode >> (28 + id)) & 0b1;
    uint8_t do_regulation = (dbg.tpa_mode >> (28 + id)) & 0b1;
    if (do_regulation) {
    	invoke_sched_vanilla_2lvl(id, real_time, acc_nominal_time, &log_ms_record);
    }

    // milestone update
    if (reported_hit[id] != ms_under_monitor[id]->address) {
        etm_disable(id);
        for (j = 0; j < 4; ++j) {
            if (new_ms->children[j].offset == 0xFFFFFFFF) {
                reached_last_child = 1;
                if (j == 0)
                    hit_last |= 0b1 << id;
            }
            if (reached_last_child) {
                etm_write_acvr(id, 7 - j, 0);
                event_address_map[id][j] = 0;
            } else {
                uint32_t addr = ((milestone *) &tmg_buf[id][new_ms->children[j].offset])->address;
                etm_write_acvr(id, 7 - j, addr);
                event_address_map[id][j] = addr;
                if (do_regulation) {
                    if (addr == 0xdeadbeef) {
                        // deadbeef indicate the end of the TMG
                    	invoke_sched_epilogue_vanilla_2lvl(id, &log_ms_record);
                    }
                }
            }
        }
        dbg.ms_updates[id]++;
        etm_enable(id);
    }

    ms_under_monitor[id] = new_ms;
    add_record(id, log_ms_record);
}

void handle_hit(uint8_t id) {
    if (reported_hit[id] == 0) {
        return;
    }

    uint32_t j = 0;

    for (j = 0; j < 4 && ms_under_monitor[id]->children[j].offset != 0xFFFFFFFF; ++j) {
        milestone * child = (milestone *) &tmg_buf[id][ms_under_monitor[id]->children[j].offset];

        if (child->address == reported_hit[id]) {
            set_new_ms_under_monitor(id, ms_under_monitor[id]->address, ms_under_monitor[id]->children[j].nominal_time, child);
            break;
        }
    }

    reported_hit[id] = 0;
}

void reset_tmg() {
    int i, j;

    for (i = 0; i < 4; ++i) {
		for (j = 0; j < TMG_BUFFER_SIZE / 4; ++j)
			tmg_buf[i][j] = 0xFF;

		reported_hit[i] = 0;
		event_address_map[i][0] = 0;
		event_address_map[i][1] = 0;

		ms_fake_parent[i].children[0].offset = 0;
		ms_fake_parent[i].children[0].nominal_time = 0;
		ms_fake_parent[i].children[1].offset = 0xFFFFFFFF;
		ms_under_monitor[i] = &ms_fake_parent[i];
    }

    tmp_cti_test = 0;
    dbg.tmg_ready = 0;

    report("tmg_buf reset, waiting for tmg graph");

    while(dbg.tmg_ready == 0);

    for (i = 0; i < 4; ++i) {
    	for (j = 0; j < 4; ++j)
    		event_address_map[i][j] = 0;

    	for (j = 0; j < 4 && ms_under_monitor[i]->children[j].offset != 0xFFFFFFFF; ++j) {
    		milestone * child = (milestone *) &tmg_buf[i][ms_under_monitor[i]->children[j].offset];
    		event_address_map[i][j] = child->address;
    	}
    }

    for (i = 0; i < 4; i++) {
        real_time_base[i] = 0;
        acc_nominal_time[i] = 0;
        entry_addr[i] = ((uint32_t *) tmg_buf[i])[0];
        rt_base_log_pt[i] = 190;
    }

    reset_sched();
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
