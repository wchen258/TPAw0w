#ifndef DBG_UTIL_H
#define DBG_UTIL_H

#include <stdint.h>
#include <stdatomic.h>

#define R0_BTCM_BASE 0xFFE20000

/* Usage convention:
 * write 1 to hotreset cause the DEF to reset
 * other registers have no constraint
 */
struct debugger {
    uint32_t hotreset_old;
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

struct inter_rpu {
    uint32_t hotreset;
	volatile atomic_flag report_lock;
	uint32_t simple_report_lock;
};

extern volatile struct inter_rpu * inter_rpu_com;
extern volatile struct debugger dbg;

void reset_inter_rpu_com(void);

void report(const char* format, ... );
void reset_dbg_util();

#endif // DBG_UTIL_H
