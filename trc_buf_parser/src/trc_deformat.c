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
uint32_t buffer_word_pointer;

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
uint8_t current_etm_id;

// Trigger a manual flush that drains all ETM(s) buffers
#define ETR_MAN_FLUSH_BIT    6
#define CS_BASE              0xFE800000
#define TMC3                 0x170000
#define FFCR                 0x304
volatile uint32_t* etr_ffcr = (volatile uint32_t*) (CS_BASE + TMC3 + FFCR);

static void etr_man_flush() {
	*etr_ffcr |= (0x1 << ETR_MAN_FLUSH_BIT);
}

static inline void debug_ptr(uint32_t point) {
	debug_flag = debug_flag | (0x1 << point);
}

uint32_t get_ptrc_buf_pointer(uint32_t i) {
	uint32_t ptrc_buf_pointer_val = (ptrc_buf_pointer[i] % (PTRC_BUFFER_SIZE / 4));
	// ptrc_buf_pointer[i] = ptrc_buf_pointer[i] + 1;

	if (ptrc_buf_pointer[i] >= (PTRC_BUFFER_SIZE / 4)) {
		debug_ptr(13);
		//report("get_ptrc_buf_pointer wrap around pause\n");
		//while(1);
	}

	return ptrc_buf_pointer_val;
}

void proc_frame(uint8_t* frame_buf, uint8_t* cur_id) {
    uint32_t i;
    uint8_t aux = frame_buf[15];

    for(i = 0; i < 8; ++i) {

        if ( (frame_buf[i*2] & 0x1) && (aux & (0x1 << i)) ) {
            // new ID and the next byte corresponding to the old ID
            if(i==7) {
            	debug_ptr(1);
            	report("PROC_FRAME ERROR: auxiliary fault!\n");
                while(1);
            }

            if (*cur_id != 0) {
            	ptrc_buf[*cur_id - 1][get_ptrc_buf_pointer(*cur_id - 1)] = frame_buf[i*2 + 1];
				ptrc_buf_pointer[*cur_id - 1] ++;
            } else {
            	// debug_ptr(15);
				report("ID0\n");
				while(1);
            }

            *cur_id = (frame_buf[i*2] & 0xfe) >> 1;
        } else if ( (frame_buf[i*2] & 0x1) && !(aux & (0x1 << i)) ) {
            // new ID and the next byte corresponding to the new ID
            *cur_id = (frame_buf[i*2] & 0xfe) >> 1;
            if(i != 7) {
            	if (*cur_id != 0) {
            		ptrc_buf[*cur_id - 1][get_ptrc_buf_pointer(*cur_id - 1)] = frame_buf[i*2 + 1];
					ptrc_buf_pointer[*cur_id - 1] ++;
            	} else {
            		debug_ptr(16);
				report("ID0\n");
				while(1);
            	}
            }
        } else {
            // Data byte
            uint8_t dat = (frame_buf[i*2] & 0xfe) | ((aux & (0x1 << i)) >> i);
            if (*cur_id != 0) {
            	ptrc_buf[*cur_id - 1][get_ptrc_buf_pointer(*cur_id - 1)] = dat;
				ptrc_buf_pointer[*cur_id - 1] ++;
            } else {
            	debug_ptr(17);
				report("ID0\n");
				while(1);
            }
            if(i != 7) {
            	if (*cur_id != 0) {
            		ptrc_buf[*cur_id - 1][get_ptrc_buf_pointer(*cur_id - 1)] = frame_buf[i*2 + 1];
					ptrc_buf_pointer[*cur_id - 1] ++;
            	} else {
            		debug_ptr(18);
				report("ID0\n");
				while(1);
            	}
            }
        }
    }
}

static void read_frame(uint32_t* frame) {
	uint32_t word;

	for (word = 0; word < 4; ++word) {
		while (etr_buffer[buffer_word_pointer] == 0xCAFECAFE) {
			debug_ptr(25);
			//etr_man_flush();
		}

		frame[word] = etr_buffer[buffer_word_pointer];
		buffer_word_pointer = (buffer_word_pointer + 1) % (ETR_BUF_SIZE / 4);

		debug_ptr(26);

		if (buffer_word_pointer == 0) {
			debug_ptr(2);
			report("PAUSED, WRAP AROUND");
			while(1);
		}
	}
}

void trace_loop(void) {
	uint32_t frame[4];

	while(1) {
		read_frame(frame);
		proc_frame((uint8_t*) frame, &current_etm_id);
	}
}

void reset(void) {
	uint32_t i;

	for (i = 0; i < ETR_BUF_SIZE / 4; ++i) {
		etr_buffer[i] = 0xCAFECAFE;
	}

	etr_buffer_control = 0;
	buffer_word_pointer = 0;
	current_etm_id = 5;

	debug_flag = 0;
}

int main()
{
    init_platform();

    xil_printf("\n\r\n\r");
    report("Started (Deformatter 1.00001)");

    reset();

    report("Reset completed, starting loop");
    report("etr_buffer address: 0x%x", etr_buffer);
    report("etr_buffer_control address: 0x%x", &etr_buffer_control);
    report("buffer_word_pointer address: 0x%x", &buffer_word_pointer);
    report("current_etm_id address: 0x%x", &current_etm_id);
    report("debug_flag address: 0x%x", &debug_flag);

    trace_loop();

    cleanup_platform();
    return 0;
}
