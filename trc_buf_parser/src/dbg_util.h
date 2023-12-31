#ifndef DBG_UTIL_H
#define DBG_UTIL_H

#include <stdint.h>

/* Usage convention:
 * write 1 to hotreset cause the DEF to reset
 * other registers have no constraint
 */
struct debugger {
    uint32_t hotreset;
	uint32_t vals[7];
	uint32_t etm_inside_disable_timer;
	uint32_t fffe_capture[8];
	uint32_t man_flush_ct;
	uint32_t man_flush_finished;
	uint32_t id_zero_ct;
	uint32_t id_other_ct;
	uint32_t max_fload_time;
	uint32_t max_inter_frame_time;
	uint32_t max_fload_id;
	uint32_t total_frames;
	uint32_t fload_times[60];
	uint32_t finter_times[60];
	uint32_t total_read_fifo;
	uint32_t fifo_read_tries;
	uint32_t fifo_read_tries_after;
	uint32_t ctiacks[4];
	uint32_t ctiacks_r5;
};

extern volatile struct debugger dbg;

void report(const char* format, ... );
void reset_dbg_util();

#endif // DBG_UTIL_H
