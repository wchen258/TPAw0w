/******************************************************************************
*
* The trc_deformat deployed on Cortex-R5 core 0.
* It constantly triggers the manual flush of ETR to poll the trace data from ETM(s) to trc_buf_zone (defined in linker)
* The etr_buf_zone size is max to be 4KB (defined in the linker)
* If multiple cores ETM(s) are active, it also responsible for deformat the formated trace stream.
*
* It would write the deformated trace stream(s) (compatible with ETM manual) to the corresponding
* trace data buffer in TMC of Cortex-R5 core 1
*
* The control registers are reserved at trc_ctrl_zone (defined in linker)
*
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include "coresight_lib.h"
#include "dbg_util.h"
#include "common.h"

#define CHECK(x,y) (((x) & (1 << (y))) ? 1 : 0)

// MISC, PRINTF
void report(const char* format, ... ) {
	va_list args;
	xil_printf("DEFORMATTER: ");
	va_start(args, format);
	xil_vprintf(format, args);
	va_end(args);
	xil_printf("\n\r");
}

volatile uint32_t debug_flag = 0;

// ETR BUFFER AND POINTERS
#define ETR_BUF_SIZE         (4096 * 8)
volatile uint32_t etr_buffer[ETR_BUF_SIZE / 4] __attribute__((section(".trc_buf_zone")));
volatile uint32_t etr_buffer_control __attribute__((section(".trc_ctrl_zone")));
uint32_t buffer_pointer;

// PTRC BUFFER AND POINTERS
#define PTRC_BUFFER_SIZE 	 4096
#define R1_BTCM_BASE         0xFFEB0000
volatile uint8_t* ptrc_buf[4] = {
	(volatile uint8_t*) (R1_BTCM_BASE + 0 * PTRC_BUFFER_SIZE / 4),
	(volatile uint8_t*) (R1_BTCM_BASE + 1 * PTRC_BUFFER_SIZE / 4),
	(volatile uint8_t*) (R1_BTCM_BASE + 2 * PTRC_BUFFER_SIZE / 4),
	(volatile uint8_t*) (R1_BTCM_BASE + 3 * PTRC_BUFFER_SIZE / 4),
};
volatile uint32_t* ptrc_buf_pointer = (volatile uint32_t*) (R1_BTCM_BASE + PTRC_BUFFER_SIZE);
//volatile uint32_t* ptrc_buf_pointer_read = (volatile uint32_t*) (R1_BTCM_BASE + PTRC_BUFFER_SIZE + 4 * 32);
uint8_t current_etm_id;

// Trigger a manual flush that drains all ETM(s) buffers
#define ETR_MAN_FLUSH_BIT    6
#define CS_BASE              0xFE800000
#define TMC3                 0x170000
#define FFCR                 0x304
#define LAR				     0xFB0   // Lock Access Register
#define LSR				     0xFB4   // Lock Status Register	
volatile uint32_t* etr_ffcr = (volatile uint32_t*) (CS_BASE + TMC3 + FFCR);
volatile uint32_t* etr_lar  = (volatile uint32_t*) (CS_BASE + TMC3 + LAR );
volatile uint32_t* etr_lsr  = (volatile uint32_t*) (CS_BASE + TMC3 + LSR );
///
volatile uint32_t* etr_ffcr1 = (volatile uint32_t*) (CS_BASE + TMC1 + FFCR);
volatile uint32_t* etr_ffcr2 = (volatile uint32_t*) (CS_BASE + TMC2 + FFCR);

///

uint8_t flush_allowed = 1;

XTime firsthword_time, lasthword_time;
XTime frame_finished = 0, frame_started = 0;

static void etr_man_flush() {
	if(CHECK(etr_buffer_control, 0)) {
		dbg.vals[7] = 0xDEADCAFE;
		//*etr_ffcr1 |= (0x1 << ETR_MAN_FLUSH_BIT);
		//*etr_ffcr2 |= (0x1 << ETR_MAN_FLUSH_BIT);
		*etr_ffcr |= (0x1 << ETR_MAN_FLUSH_BIT);

		//while(*etr_ffcr & (0x1 << ETR_MAN_FLUSH_BIT));
		//usleep(50);

		dbg.man_flush_ct ++ ;
	}
}

uint32_t get_ptrc_buf_pointer(uint32_t i) {
	/*while (1) {
		uint32_t read_pointer = ptrc_buf_pointer[i + 4];
		if (read_pointer < ptrc_buf_pointer[i]) {
			if (ptrc_buf_pointer[i] - read_pointer < ((PTRC_BUFFER_SIZE / 4) - 64)) {
				break;
			}
		} else {
			break;
		}
	}*/

	uint32_t ptrc_buf_pointer_val = (ptrc_buf_pointer[i] % (PTRC_BUFFER_SIZE / 4));
	// ptrc_buf_pointer[i] = ptrc_buf_pointer[i] + 1;

	//if (ptrc_buf_pointer[i] >= (PTRC_BUFFER_SIZE / 4)) {
		//report("get_ptrc_buf_pointer wrap around pause\n");
		//while(1);
	//}

	return ptrc_buf_pointer_val;
}

/*
static void write_to_regulator_buffer(uint8_t* cur_id, uint8_t data) {
    if (*cur_id >= 1 && *cur_id <= 4) {
    	uint32_t write_pointer = get_ptrc_buf_pointer(*cur_id - 1);

    	ptrc_buf[*cur_id - 1][write_pointer] = data;
    	ptrc_buf[1][write_pointer] = dbg.total_frames;

		ptrc_buf_pointer[*cur_id - 1] ++;
    } else if (*cur_id == 0) {
    	SET(dbg.vals[0], 1);
    	dbg.id_zero_ct ++ ;
    } else if (*cur_id > 4) {
    	SET(dbg.vals[0], 2);
    	dbg.id_other_ct ++ ;
    } else {
    	SET(dbg.vals[0], 3);
    	dbg.id_other_ct ++ ;
    }
}
*/

/*
void proc_frame(uint8_t* frame_buf, uint8_t* cur_id) {
    uint32_t i;
    uint8_t aux = frame_buf[15];

    for(i = 0; i < 8; ++i) {

        if ( (frame_buf[i*2] & 0x1) && (aux & (0x1 << i)) ) {
            if(i==7) {
            	report("PROC_FRAME ERROR: auxiliary fault!\n");
            	SET(dbg.vals[5], 0);
                while(1);
            }

            write_to_regulator_buffer(cur_id, frame_buf[i*2 + 1]);

            *cur_id = (frame_buf[i*2] & 0xfe) >> 1;
        } else if ( (frame_buf[i*2] & 0x1) && !(aux & (0x1 << i)) ) {
            *cur_id = (frame_buf[i*2] & 0xfe) >> 1;
            if(i != 7) {
            	write_to_regulator_buffer(cur_id, frame_buf[i*2 + 1]);
            }
        } else {
            uint8_t dat = (frame_buf[i*2] & 0xfe) | ((aux & (0x1 << i)) >> i);
            write_to_regulator_buffer(cur_id, dat);

            if(i != 7) {
            	write_to_regulator_buffer(cur_id, frame_buf[i*2 + 1]);
            }
        }
    }
}
*/

/*

OLD COMMENT

static void read_frame(uint32_t* frame, uint32_t word_num) {
	uint32_t word;

	for (word = 0; word < word_num; ++word) {
		while (etr_buffer[buffer_pointer] == 0xCAFECAFE) {
			etr_man_flush();
		}

		frame[word] = etr_buffer[buffer_pointer];
		buffer_pointer = (buffer_pointer + 1) % (ETR_BUF_SIZE / 4);

		if (buffer_pointer == 0) {
			report("PAUSED, WRAP AROUND");
			SET(dbg.vals[6], 2);
			while(1);
		}
	}
}*/

static void read_bytes(uint8_t* output, uint32_t count) {
	uint32_t byte;

	for (byte = 0; byte < count; ++byte) {
		while (etr_buffer[buffer_pointer / 4] == 0xCAFECAFE) {
			//if (flush_allowed) {
				etr_man_flush();
			//	flush_allowed = 0;
			//}
		}

		output[byte] = ((uint8_t *) etr_buffer)[buffer_pointer];
		buffer_pointer = (buffer_pointer + 1) % (ETR_BUF_SIZE);

		if (buffer_pointer == 0) {
			report("PAUSED, WRAP AROUND");
			SET(dbg.vals[6], 2);
			while(1);
		}
	}
}

/*
 * BY-PASS MODE
 */

/*
static void read_frame(uint32_t* frame) {
	uint32_t word;

	for (word = 0; word < 1; ++word) {
		while (etr_buffer[buffer_pointer] == 0xCAFECAFE) {
			etr_man_flush();
		}

		frame[word] = etr_buffer[buffer_pointer];
		buffer_pointer = (buffer_pointer + 1) % (ETR_BUF_SIZE / 4);

		if (buffer_pointer == 0) {
			report("PAUSED, WRAP AROUND");
			SET(dbg.vals[0], 2);
			while(1);
		}
	}
}
*/

void proc_frame(uint8_t* frame_buf, uint8_t* cur_id) {
	uint32_t i;

	for (i = 0; i < 4; ++i) {
		ptrc_buf[0][get_ptrc_buf_pointer(0)] = frame_buf[i];
		ptrc_buf_pointer[0] ++;
	}
}

void trace_loop(void) {
	uint32_t frame;

	while(1) {
		read_bytes((uint8_t*) &frame, 4);
		proc_frame((uint8_t*) &frame, &current_etm_id);
	}
}

/*
void trace_loop(void) {
	uint8_t frame[16];
	uint32_t halfword_count = 0;

	while(1) {
		flush_allowed = 1;

		for (halfword_count = 0; halfword_count < 8; ++halfword_count) {
			read_bytes(&frame[halfword_count * 2], 2);

			if (halfword_count == 0) {
				XTime_GetTime(&firsthword_time);
				XTime_GetTime(&frame_started);

				if (frame_finished != 0) {
					uint32_t inter_frame_time = (frame_started - frame_finished) / COUNTS_PER_USECOND;
					if (dbg.max_inter_frame_time < inter_frame_time) {
						dbg.max_inter_frame_time = inter_frame_time;
					}
				}
			} else if (halfword_count == 7) {
				XTime_GetTime(&lasthword_time);

				uint32_t fload_time = (lasthword_time - firsthword_time) / COUNTS_PER_USECOND;
				if (dbg.max_fload_time < fload_time) {
					dbg.max_fload_time = fload_time;
					dbg.max_fload_id = dbg.total_frames;
				}

				dbg.fload_times[dbg.total_frames] = fload_time;

				XTime_GetTime(&frame_finished);
				dbg.total_frames++;

				if (dbg.total_frames == 59) {
					while(1);
				}
			}
		}

		proc_frame((uint8_t*) &frame[0], &current_etm_id);
	}
}
*/

void reset(void) {
	uint32_t i;

	for (i = 0; i < ETR_BUF_SIZE / 4; ++i) {
		etr_buffer[i] = 0xCAFECAFE;
	}

	for (i = 0; i < 8; ++i) {
		dbg.fffe_capture[i] = 0;
	}

	etr_buffer_control = 0;
	buffer_pointer = 0;
	current_etm_id = 5;

	debug_flag = 0;
}

int main()
{
    init_platform();

    xil_printf("\n\r\n\r");
    report("Started (USELESS Deformatter 1.1, MP. Ctrl triple Flush, Frame Pass over, Flush Control)");

    reset();

    report("Reset completed, starting loop");
    report("etr_buffer address: 0x%x", etr_buffer);
    report("etr_buffer_control address: 0x%x", &etr_buffer_control);
    report("buffer_pointer address: 0x%x", &buffer_pointer);
    report("current_etm_id address: 0x%x", &current_etm_id);

    trace_loop();

    cleanup_platform();
    return 0;
}
