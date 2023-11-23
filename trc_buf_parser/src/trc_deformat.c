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

#define CHECK(x, y) (((x) & (1 << (y))) ? 1 : 0)

// MISC, PRINTF
void report(const char *format, ...)
{
    va_list args;
    xil_printf("DEFORMATTER: ");
    va_start(args, format);
    xil_vprintf(format, args);
    va_end(args);
    xil_printf("\n\r");
}

volatile uint32_t debug_flag = 0;

// ETR BUFFER AND POINTERS
#define ETR_BUF_SIZE (4096 * 8)
volatile uint32_t etr_buffer[ETR_BUF_SIZE / 4] __attribute__((section(".trc_buf_zone")));
volatile uint32_t etr_buffer_control __attribute__((section(".trc_ctrl_zone")));
uint32_t buffer_pointer;

// PTRC BUFFER AND POINTERS
#define PTRC_BUFFER_SIZE 4096
#define R1_BTCM_BASE 0xFFEB0000
volatile uint8_t *ptrc_buf[4] = {
    (volatile uint8_t *)(R1_BTCM_BASE + 0 * PTRC_BUFFER_SIZE / 4),
    (volatile uint8_t *)(R1_BTCM_BASE + 1 * PTRC_BUFFER_SIZE / 4),
    (volatile uint8_t *)(R1_BTCM_BASE + 2 * PTRC_BUFFER_SIZE / 4),
    (volatile uint8_t *)(R1_BTCM_BASE + 3 * PTRC_BUFFER_SIZE / 4),
};
volatile uint32_t *ptrc_buf_pointer = (volatile uint32_t *)(R1_BTCM_BASE + PTRC_BUFFER_SIZE);
// volatile uint32_t* ptrc_buf_pointer_read = (volatile uint32_t*) (R1_BTCM_BASE + PTRC_BUFFER_SIZE + 4 * 32);
uint8_t current_etm_id;

// Trigger a manual flush that drains all ETM(s) buffers
#define ETR_MAN_FLUSH_BIT 6
#define ETR_DRAIN_BUFFER_BIT 14
#define CS_BASE 0xFE800000
#define TMC3 0x170000
#define STS 0x00C
#define FFSR 0x300
#define FFCR 0x304
#define LAR 0xFB0 // Lock Access Register
#define LSR 0xFB4 // Lock Status Register
#define RRD 0x010 // RAM Read Data Register
#define RRP 0x014 // RAM Read Pointer
#define RWP 0x018 // RAM Write Pointer
#define CBUFLEVEL 0x030

/*
volatile uint32_t* etr_sts  = (volatile uint32_t*) (CS_BASE + TMC3 + STS);
volatile uint32_t* etr_ctl  = (volatile uint32_t*) (CS_BASE + TMC3 + 0x020);
volatile uint32_t* etr_ffsr = (volatile uint32_t*) (CS_BASE + TMC3 + FFSR);
volatile uint32_t* etr_ffcr = (volatile uint32_t*) (CS_BASE + TMC3 + FFCR);
volatile uint32_t* etr_lar  = (volatile uint32_t*) (CS_BASE + TMC3 + LAR );
volatile uint32_t* etr_lsr  = (volatile uint32_t*) (CS_BASE + TMC3 + LSR );
volatile uint32_t* etr_rwp  = (volatile uint32_t*) (CS_BASE + TMC3 + RWP );
volatile uint32_t* etr_rrd  = (volatile uint32_t*) (CS_BASE + TMC3 + RRD );
///
volatile uint32_t* etr_ffcr1 = (volatile uint32_t*) (CS_BASE + TMC1 + FFCR);
volatile uint32_t* etr_ffcr2 = (volatile uint32_t*) (CS_BASE + TMC2 + FFCR);
*/

volatile uint32_t *etr_sts = (volatile uint32_t *)(CS_BASE + TMC1 + STS);
volatile uint32_t *etr_ffcr = (volatile uint32_t *)(CS_BASE + TMC1 + FFCR);
volatile uint32_t *etr_rrd = (volatile uint32_t *)(CS_BASE + TMC1 + RRD);

volatile uint32_t *etr_cbuflevel = (volatile uint32_t *)(CS_BASE + TMC1 + CBUFLEVEL);

volatile uint32_t *cti_intack = (volatile uint32_t *)(CS_BASE + R5_0_CTI + 0x010);
volatile uint32_t *cti_trigoutstatus = (volatile uint32_t *)(CS_BASE + R5_0_CTI + 0x134);

volatile uint32_t *a0_cti_intack = (volatile uint32_t *)(CS_BASE + A53_0_CTI + 0x010);
volatile uint32_t *a0_cti_trigoutstatus = (volatile uint32_t *)(CS_BASE + A53_0_CTI + 0x134);

volatile uint32_t *a1_cti_intack = (volatile uint32_t *)(CS_BASE + A53_1_CTI + 0x010);
volatile uint32_t *a1_cti_trigoutstatus = (volatile uint32_t *)(CS_BASE + A53_1_CTI + 0x134);

volatile uint32_t *a2_cti_intack = (volatile uint32_t *)(CS_BASE + A53_2_CTI + 0x010);
volatile uint32_t *a2_cti_trigoutstatus = (volatile uint32_t *)(CS_BASE + A53_2_CTI + 0x134);

volatile uint32_t *a3_cti_intack = (volatile uint32_t *)(CS_BASE + A53_3_CTI + 0x010);
volatile uint32_t *a3_cti_trigoutstatus = (volatile uint32_t *)(CS_BASE + A53_3_CTI + 0x134);


uint8_t flush_allowed = 1;

// XTime firsthword_time, lasthword_time;
// XTime frame_finished = 0, frame_started = 0;

static void etr_man_flush()
{
    if (CHECK(etr_buffer_control, 0))
    {
        dbg.man_flush_finished++;
        *etr_ffcr |= (0x1 << ETR_MAN_FLUSH_BIT);
        while (*etr_ffcr & (0x1 << ETR_MAN_FLUSH_BIT))
            ;
    }
}

static void check_ctitrigger_status()
{
    if (((*cti_intack >> 4) & 0b1) == 1)
    {
        *cti_trigoutstatus |= (0b1 << 4);

        // etr_man_flush();

        dbg.ctiacks_r5++;
    }

    /*
    if (((*a0_cti_trigoutstatus >> 3) & 0b1) == 1) {
        *a0_cti_intack |= (0b1 << 3);
        dbg.ctiacks[0]++;
    }
    if (((*a1_cti_trigoutstatus >> 3) & 0b1) == 1) {
        *a1_cti_intack |= (0b1 << 3);
        dbg.ctiacks[1]++;
    }
    if (((*a2_cti_trigoutstatus >> 3) & 0b1) == 1) {
        *a2_cti_intack |= (0b1 << 3);
        dbg.ctiacks[2]++;
    }
    if (((*a3_cti_trigoutstatus >> 3) & 0b1) == 1) {
        *a3_cti_intack |= (0b1 << 3);
        dbg.ctiacks[3]++;
    }
    */
}


uint32_t get_ptrc_buf_pointer(uint32_t i)
{
    while (1)
    {
        uint32_t read_pointer = ptrc_buf_pointer[i + 4];
        if (read_pointer < ptrc_buf_pointer[i])
        {
            if (ptrc_buf_pointer[i] - read_pointer < ((PTRC_BUFFER_SIZE / 4) - 64))
            {
                break;
            }
        }
        else
        {
            break;
        }

        dbg.vals[1]++;
    }

    uint32_t ptrc_buf_pointer_val = (ptrc_buf_pointer[i] % (PTRC_BUFFER_SIZE / 4));
    // ptrc_buf_pointer[i] = ptrc_buf_pointer[i] + 1;

    // if (ptrc_buf_pointer[i] >= (PTRC_BUFFER_SIZE / 4)) {
    // report("get_ptrc_buf_pointer wrap around pause\n");
    // while(1);
    //}

    return ptrc_buf_pointer_val;
}

static void write_to_regulator_buffer(uint8_t *cur_id, uint8_t data)
{
    if (*cur_id >= 1 && *cur_id <= 4)
    {
        uint32_t write_pointer = get_ptrc_buf_pointer(*cur_id - 1);

        ptrc_buf[*cur_id - 1][write_pointer] = data;
        // ptrc_buf[1][write_pointer] = dbg.total_frames;

        ptrc_buf_pointer[*cur_id - 1]++;
    }
    else if (*cur_id == 0)
    {
        dbg.id_zero_ct++;
    }
    else if (*cur_id > 4)
    {
        dbg.id_other_ct++;
    }
    else
    {
        dbg.id_other_ct++;
        dbg.vals[3] = *cur_id;
    }
}

void proc_frame(uint8_t *frame_buf, uint8_t *cur_id)
{
    uint32_t i;
    uint8_t aux = frame_buf[15];

    for (i = 0; i < 8; ++i)
    {

        if ((frame_buf[i * 2] & 0x1) && (aux & (0x1 << i)))
        {
            if (i == 7)
            {
                report("PROC_FRAME ERROR: auxiliary fault!\n");
                SET(dbg.vals[5], 0);
                while (1)
                    ;
            }

            write_to_regulator_buffer(cur_id, frame_buf[i * 2 + 1]);

            *cur_id = (frame_buf[i * 2] & 0xfe) >> 1;
        }
        else if ((frame_buf[i * 2] & 0x1) && !(aux & (0x1 << i)))
        {
            *cur_id = (frame_buf[i * 2] & 0xfe) >> 1;
            if (i != 7)
            {
                write_to_regulator_buffer(cur_id, frame_buf[i * 2 + 1]);
            }
        }
        else
        {
            uint8_t dat = (frame_buf[i * 2] & 0xfe) | ((aux & (0x1 << i)) >> i);
            write_to_regulator_buffer(cur_id, dat);

            if (i != 7)
            {
                write_to_regulator_buffer(cur_id, frame_buf[i * 2 + 1]);
            }
        }
    }
}



// FORMATTED FIFO MODE

XTime last_flush = 0;

static void read_word(uint32_t *output)
{
    uint32_t word = 0xFFFFFFFF;

    do
    {
        dbg.fifo_read_tries++;
        // etr_man_flush();
        // check_ctitrigger_status();

        word = *etr_rrd;

        if (etr_buffer_control == 0)
        {
            if (word == 0xFFFFFFFF && ((*etr_sts >> 2) & 0x1))
            {
                dbg.vals[7] = 0xD0DCAFEA;

                report("STOPPED, word==0xFFFFFFFF and TMCReady");
                while (1)
                    ;
            }
        }
        else if (word == 0xFFFFFFFF)
        {
            dbg.vals[4]++;
        }
        dbg.fifo_read_tries_after++;
    } while (word == 0xFFFFFFFF && dbg.hotreset == 0);

    dbg.total_read_fifo++;

    // if (buffer_pointer < ETR_BUF_SIZE / 4)
    //	etr_buffer[buffer_pointer++] = word;

    *output = word;
}


static uint32_t idzero_ignore = 0;

void trace_loop(void)
{
    /*while (etr_buffer_control == 0);

    while (etr_buffer_control) {
        dbg.total_read_fifo++;
        check_ctitrigger_status();

        //uint32_t word = *etr_rrd;
    }*/

    uint32_t frame[4];

    while (etr_buffer_control == 0)
        ;

    while (1)
    {
        read_word(&frame[0]);
        read_word(&frame[1]);
        read_word(&frame[2]);
        read_word(&frame[3]);
        if (dbg.hotreset) {
            report("DEF hotreset");
            break;
        }
        proc_frame((uint8_t *)&frame[0], &current_etm_id);
    }
}


void reset(void)
{
    uint32_t i;

    for (i = 0; i < ETR_BUF_SIZE / 4; ++i)
    {
        etr_buffer[i] = 0xCAFECAFE;
    }

    for (i = 0; i < 8; ++i)
    {
        dbg.fffe_capture[i] = 0;
    }

    etr_buffer_control = 0;
    buffer_pointer = 0;
    current_etm_id = 5;

    debug_flag = 0;
    flush_allowed = 1;
    reset_dbg_util();
}

int main()
{
    init_platform();

    xil_printf("\n\r\n\r");
    report("Started (Deformatter 1.2, Soft, FIFO)");

    while (1)
    {
        reset();
        report("Reset completed");
        report("etr_buffer address: 0x%x", etr_buffer);
        report("etr_buffer_control address: 0x%x", &etr_buffer_control);
        report("buffer_pointer address: 0x%x", &buffer_pointer);
        report("current_etm_id address: 0x%x", &current_etm_id);
        report("dbg address: 0x%x", &dbg);
        trace_loop();
    }

    cleanup_platform();
    return 0;
}
