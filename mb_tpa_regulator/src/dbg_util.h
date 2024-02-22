#ifndef DBG_UTIL_H
#define DBG_UTIL_H

#include <stdint.h>
#include <stdatomic.h>
#include "xtime_l.h"

#define R0_BTCM_BASE 0xFFE20000

struct debugger {
	uint32_t fault;
	uint32_t tmg_ready;
	uint32_t hotreset_old;
	uint32_t tpa_mode; // control which timing attribute is logged 0 means normal (log real time), 1 means logging accumulative nominal, 2 means logging tail time
	// float    alpha;
	// float    beta;
	// uint32_t margin;
	// uint32_t vals[3];
	// uint32_t etm_inside_disable_timer;
	// uint32_t current_monitoring0;
    float alpha[4];
    float beta[4];
	uint32_t expected_solo_end_time[4]; // when this one is set, the alpha and beta should be updated
	uint32_t deadline[4];
	uint32_t trace_on_timings[3];
	uint32_t traceon_frames[4];
	uint32_t ms_updates[4];
	uint32_t event;
	uint32_t event_count[4][4];
	XTime    milestone_timestamps[4][200];
    // uint64_t pmcc[4][100];
	uint32_t ms_ts_pt[4];
	// char log[1024];
	uint32_t overflow_ct;
	uint32_t enter_dbg_ct;
	uint32_t leave_dbg_ct;
};

struct inter_rpu {
    uint32_t hotreset;
	volatile atomic_flag report_lock;
	uint32_t simple_report_lock;
};

extern volatile struct debugger dbg;
extern volatile struct inter_rpu * inter_rpu_com;

void report(const char* format, ... );
void breport(const char* format, ...);
void reset_dbg_util();

#endif // DBG_UTIL_H
