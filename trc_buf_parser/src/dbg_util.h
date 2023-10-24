#ifndef DBG_UTIL_H
#define DBG_UTIL_H

#include <stdint.h>

/* Usage convention:
 * vals[0]: faulty assertion. Set bit to indicate an unintended behavior occur
 */
struct debugger {
	uint32_t vals[8];
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
};

extern volatile struct debugger dbg;

void report(const char* format, ... );

#endif // DBG_UTIL_H
