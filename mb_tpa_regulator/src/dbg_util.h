#ifndef DBG_UTIL_H
#define DBG_UTIL_H

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

#endif // DBG_UTIL_H
