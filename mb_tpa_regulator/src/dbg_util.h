#ifndef DBG_UTIL_H
#define DBG_UTIL_H

#include <stdint.h>
#include "xtime_l.h"

struct debugger {
	uint32_t fault;
	uint32_t tmg_ready;
	uint32_t hotreset;
	uint32_t assert;
	uint32_t sync_ct;
	uint32_t on_ct;
	uint32_t vals[4];
	uint32_t etm_inside_disable_timer;
	uint32_t current_monitoring[4];
	uint32_t trace_on_timings[8];
	uint32_t traceon_frames[4];
	uint32_t ms_updates[4];
	uint32_t event;
	uint32_t event_count[4][4];
	XTime    milestone_timestamps[4][200];
    // uint64_t pmcc[4][100];
	uint32_t ms_ts_pt[4];
	// char log[1024];
	uint32_t overflow_ct;
};

extern volatile struct debugger dbg;

void report(const char* format, ... );
void breport(const char* format, ...);
void reset_dbg_util();

#endif // DBG_UTIL_H
