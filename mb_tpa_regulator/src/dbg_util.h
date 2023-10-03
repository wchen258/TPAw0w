#ifndef DBG_UTIL_H
#define DBG_UTIL_H

#include <stdint.h>

#define SET(x, y) ((x) |= (1 << (y)))

struct debugger {
	uint32_t select;
	uint32_t tmg_ready;
	uint32_t fault;
	uint32_t assert;
	uint32_t sync_ct;
	uint32_t on_ct;
	uint32_t vals[4];
};

extern volatile uint32_t debug_flag;
extern uint32_t debug_count1[4];
extern uint32_t debug_count2[4];
extern uint32_t debug_count3[4];
extern volatile struct debugger dbg;

void report(const char* format, ... );
void debug_ptr(uint32_t point);

#endif // DBG_UTIL_H
