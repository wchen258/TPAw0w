#include <stdint.h>
#include "coresight_lib.h"
#include "common.h"
#include "dbg_util.h"
#include "xtime_l.h"

static XTime etm_off, etm_resume;

static uint32_t pmu_cycle_counter_addrs[] = {
    CS_BASE + A53_0_PMU + PMCCNTR_EL0, 
    CS_BASE + A53_1_PMU + PMCCNTR_EL0, 
    CS_BASE + A53_2_PMU + PMCCNTR_EL0, 
    CS_BASE + A53_3_PMU + PMCCNTR_EL0, 
};

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

static uint32_t a53_cti_pulse_addrs [] = {
		CS_BASE + A53_0_CTI + 0x01C,
		CS_BASE + A53_1_CTI + 0x01C,
		CS_BASE + A53_2_CTI + 0x01C,
		CS_BASE + A53_3_CTI + 0x01C
};

static uint32_t a53_cti_ack_addrs [] = {
		CS_BASE + A53_0_CTI + 0x010,
		CS_BASE + A53_1_CTI + 0x010,
		CS_BASE + A53_2_CTI + 0x010,
		CS_BASE + A53_3_CTI + 0x010
};

#define ETR_MAN_FLUSH_BIT    6
volatile uint32_t* etr_ffcr = (volatile uint32_t*) (CS_BASE + TMC1 + FFCR);

#define CHECK(x,y) (((x) & (1 << (y))) ? 1 : 0)


static void etr_man_flush() {
	*etr_ffcr |= (0x1 << ETR_MAN_FLUSH_BIT);
	while(*etr_ffcr & (0x1 << ETR_MAN_FLUSH_BIT));
}


/*  enabling is non-blocking. It returns even when ETM is not on yet.
    This should cause no problem. Since ETM would be on shortly
    uncomment the whle(CHECK) loop to make it blocking
*/
void etm_enable(uint8_t id) {
    volatile uint32_t *ctrl = (uint32_t *) etm_ctrl_addrs[id];
    volatile uint32_t *stat = (uint32_t *) etm_stat_addrs[id];

    SET(*ctrl, 0);
//    while(CHECK(*stat, 0) == 1);

    XTime_GetTime(&etm_resume);

    if (etm_resume > etm_off) {
    	// uint32_t new_time = (etm_resume - etm_off) / COUNTS_PER_USECOND;
    	uint32_t new_time = (etm_resume - etm_off) ;
    	if (new_time > dbg.etm_inside_disable_timer)
    		dbg.etm_inside_disable_timer = new_time;
    }

//    etr_man_flush();
}

/*  Disable is blocking. ETM has to be in off-ready state to be programmed */
void etm_disable(uint8_t id) {
	//etr_man_flush();

    volatile uint32_t *ctrl = (uint32_t *) etm_ctrl_addrs[id];
    volatile uint32_t *stat = (uint32_t *) etm_stat_addrs[id];

    XTime_GetTime(&etm_off);

    CLEAR(*ctrl, 0);
    while(CHECK(*stat, 0) == 0);
}

/*  only used when ETM in off-ready state. */
void etm_write_acvr(uint8_t id, uint8_t ac_id, uint32_t addr_val) {
    uint32_t *acvr = (uint32_t *) etm_acvr_addrs[id][ac_id];
    *acvr = addr_val;
}

/* write a address comparator pair */
void etm_write_acvr_pair(uint8_t id, uint8_t ac_id, uint32_t addr_val) {
    etm_write_acvr(id, 2 * ac_id, addr_val);
    etm_write_acvr(id, 2 * ac_id + 1, addr_val);
}

/* Read PMU cycle counter from A53_0_PMU
 * ---------------------------
 * due to RPU's bus width is 32-bit, the read has to be done
 * by reading the two 32-bit registers separately and then combine them
 * 
 * Return: 64-bit cycle counter value
*/
uint64_t read_pmu_cycle_counter() {
    uint32_t *pmu_cc_low = (uint32_t *) pmu_cycle_counter_addrs[0];
    uint32_t *pmu_cc_high = (uint32_t *) (pmu_cycle_counter_addrs[0] + 4);
    uint64_t pmu_cc = *pmu_cc_high;
    pmu_cc = (pmu_cc << 32) | *pmu_cc_low;
    return pmu_cc;
}

void a53_0_enter_dbg() {
	 uint32_t* pulse_reg = (uint32_t*) a53_cti_pulse_addrs[0];
	 uint32_t* ack_reg = (uint32_t*) a53_cti_ack_addrs[0];
	 *pulse_reg = 0b0100;
	 *ack_reg = 0b1 ;
}

void a53_0_leave_dbg() {
	uint32_t* pulse_reg = (uint32_t*) a53_cti_pulse_addrs[0];
	uint32_t* ack_reg = (uint32_t*) a53_cti_ack_addrs[0];
	*pulse_reg = 0b0010;
	*ack_reg = 0b1 << 1;
}

/*
void etr_enable() {
	volatile uint32_t * ctrl = (uint32_t *) (CS_BASE + TMC3 + CTL);
	*ctrl = 1;
}

void etr_disable() {
	volatile uint32_t * ctrl = (uint32_t *) (CS_BASE + TMC3 + CTL);
	*ctrl = 0;
}
*/
