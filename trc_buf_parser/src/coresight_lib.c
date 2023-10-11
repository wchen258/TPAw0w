#include <stdint.h>
#include "coresight_lib.h"
#include "common.h"
#include "dbg_util.h"
#include "xtime_l.h"

static uint32_t etm_ctrl_addrs[] = {
    CS_BASE + A53_0_ETM + TRCPRGCTLR, 
    CS_BASE + A53_1_ETM + TRCPRGCTLR, 
    CS_BASE + A53_2_ETM + TRCPRGCTLR, 
    CS_BASE + A53_3_ETM + TRCPRGCTLR, 
};

static uint32_t etm_stat_addrs[] = {
    CS_BASE + A53_0_ETM + TRCSTATR,
    CS_BASE + A53_1_ETM + TRCSTATR,
    CS_BASE + A53_2_ETM + TRCSTATR,
    CS_BASE + A53_3_ETM + TRCSTATR,
};

static uint32_t etm_acvr_addrs[][8] = {
    {
        CS_BASE + A53_0_ETM + TRCACVR0,
        CS_BASE + A53_0_ETM + TRCACVR1,
        CS_BASE + A53_0_ETM + TRCACVR2,
        CS_BASE + A53_0_ETM + TRCACVR3,
        CS_BASE + A53_0_ETM + TRCACVR4,
        CS_BASE + A53_0_ETM + TRCACVR5,
        CS_BASE + A53_0_ETM + TRCACVR6,
        CS_BASE + A53_0_ETM + TRCACVR7,
    },
    {
        CS_BASE + A53_1_ETM + TRCACVR0,
        CS_BASE + A53_1_ETM + TRCACVR1,
        CS_BASE + A53_1_ETM + TRCACVR2,
        CS_BASE + A53_1_ETM + TRCACVR3,
        CS_BASE + A53_1_ETM + TRCACVR4,
        CS_BASE + A53_1_ETM + TRCACVR5,
        CS_BASE + A53_1_ETM + TRCACVR6,
        CS_BASE + A53_1_ETM + TRCACVR7,
    },
    {
        CS_BASE + A53_2_ETM + TRCACVR0,
        CS_BASE + A53_2_ETM + TRCACVR1,
        CS_BASE + A53_2_ETM + TRCACVR2,
        CS_BASE + A53_2_ETM + TRCACVR3,
        CS_BASE + A53_2_ETM + TRCACVR4,
        CS_BASE + A53_2_ETM + TRCACVR5,
        CS_BASE + A53_2_ETM + TRCACVR6,
        CS_BASE + A53_2_ETM + TRCACVR7,
    },
    {
        CS_BASE + A53_3_ETM + TRCACVR0,
        CS_BASE + A53_3_ETM + TRCACVR1,
        CS_BASE + A53_3_ETM + TRCACVR2,
        CS_BASE + A53_3_ETM + TRCACVR3,
        CS_BASE + A53_3_ETM + TRCACVR4,
        CS_BASE + A53_3_ETM + TRCACVR5,
        CS_BASE + A53_3_ETM + TRCACVR6,
        CS_BASE + A53_3_ETM + TRCACVR7,
    },
};


/*  enabling is non-blocking. It returns even when ETM is not on yet.
    This should cause no problem. Since ETM would be on shortly
    uncomment the whle(CHECK) loop to make it blocking
*/
void etm_enable(uint8_t id) {
    uint32_t *ctrl = (uint32_t *) etm_ctrl_addrs[id];
//    uint32_t *stat = (uint32_t *) etm_stat_addrs[id];
    SET(*ctrl, 0);
    //while(CHECK(*stat, 0) == 1);
}

/*  Disable is blocking. ETM has to be in off-ready state to be programmed */
void etm_disable(uint8_t id) {
    volatile uint32_t *ctrl = (uint32_t *) etm_ctrl_addrs[id];
    volatile uint32_t *stat = (uint32_t *) etm_stat_addrs[id];
    CLEAR(*ctrl, 0);
    while(CHECK(*stat, 0) == 0);
}

//static volatile uint8_t *prog_ctrl = CS_BASE + A53_0_ETM + TRCPRGCTLR;
//static volatile uint8_t *trace_status = CS_BASE + A53_0_ETM + TRCSTATR;
//void etm_disable(uint8_t dummy) {
//	XTime start, end;
//	*prog_ctrl = 0x0;
//	XTime_GetTime(&start);
//	while (!(*trace_status & 0x1));
//	XTime_GetTime(&end);
//	dbg.etm_inside_disable_timer = (end - start) / COUNTS_PER_USECOND;
//	dbg.vals[0] = 0xdeadbeef;
//}


/*  only used when ETM in off-ready state. */
static void etm_write_acvr(uint8_t id, uint8_t ac_id, uint32_t addr_val) {
    uint32_t *acvr = (uint32_t *) etm_acvr_addrs[id][ac_id];
    *acvr = addr_val;
}

/* write a address comparator pair */
void etm_write_acvr_pair(uint8_t id, uint8_t ac_id, uint32_t addr_val) {
    etm_write_acvr(id, 2 * ac_id, addr_val);
    etm_write_acvr(id, 2 * ac_id + 1, addr_val);
}

void etr_enable() {
	uint32_t * ctrl = (uint32_t *) (CS_BASE + TMC3 + CTL);
	*ctrl = 1;
}

void etr_disable() {
	uint32_t * ctrl = (uint32_t *) (CS_BASE + TMC3 + CTL);
	*ctrl = 0;
}
