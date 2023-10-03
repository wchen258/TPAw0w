#include <stdint.h>
#include "tmg_monitor.h"
#include "dbg_util.h"

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


// ETM_CONTROLS START

void etm_disable(uint8_t core_id) {
}

void etm_enable(uint8_t core_id) {
}

void set_addr_cmp(uint8_t core_id, uint32_t address, int pair_id) {
}

// ETM_CONTROLS END




void report_address_hit(uint8_t id, uint32_t address) {
    reported_hit[id] = address;
}

static set_new_ms_under_monitor(uint8_t id, uint32_t nominal_time, milestone * new_ms) {
    //TODO: handle regulation logic
    
    uint8_t j, reached_last_child = 0;
    etm_disable(id);

    for (j = 0; j < 4; ++j) {
        if (new_ms->children[j].offset == 0xFFFFFFFF)
            reached_last_child = 1;
            
        if (reached_last_child) {
            set_addr_cmp(id, 0, j);
        } else {
            set_addr_cmp(id, ((milestone *) &tmg_buf[id][new_ms->children[j].offset])->address, j);
        }
    }

    etm_enable(id);

    ms_under_monitor[id] = new_ms;
}

void handle_hit(uint8_t id) {
    if (reported_hit[id] == 0)
        return;

    uint32_t j = 0;

    for (j = 0; j < 4 && ms_under_monitor[id]->children[j].offset != 0xFFFFFFFF; ++j) {
        milestone * child = (milestone *) &tmg_buf[id][ms_under_monitor[id]->children[j].offset];

        if (child->address == reported_hit[id]) {
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

		ms_under_monitor[i] = (milestone *) &tmg_buf[i][0];
    }

    dbg.tmg_ready = 0;

    report("tmg_buf reset, waiting for tmg graph");

    while(dbg.tmg_ready == 0);
}

void report_tmg_mem() {
    report("TMG MEMO GROUP START");
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
