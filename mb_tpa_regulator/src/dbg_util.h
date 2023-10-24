#ifndef DBG_UTIL_H
#define DBG_UTIL_H

#include <stdint.h>

struct debugger {
	uint32_t fault;
	uint32_t tmg_ready;
	uint32_t select;
	uint32_t assert;
	uint32_t sync_ct;
	uint32_t on_ct;
	uint32_t vals[4];
	uint32_t etm_inside_disable_timer;
	uint32_t current_monitoring[4];
	uint32_t trace_on_timings[8];
	uint32_t traceon_frames[4];
	uint32_t ms_updates[4];
};

extern volatile struct debugger dbg;

void report(const char* format, ... );

#endif // DBG_UTIL_H
