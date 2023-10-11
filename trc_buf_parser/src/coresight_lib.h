#ifndef CORESIGHT_LIB_H
#define CORESIGHT_LIB_H

#define CS_BASE 0xFE800000

/* Component offset group start */
#define ROM 0x0
#define TSGEN 0x100000
#define FUNNEL0 0x110000
#define FUNNEL1 0x120000
#define FUNNEL2 0x130000
#define TMC1 0x140000
#define TMC2 0x150000
#define REPLIC 0x160000
#define TMC3 0x170000
#define TPIU 0x180000
#define CTI0 0x190000
#define CTI1 0x1A0000
#define CTI2 0x1B0000
#define STM 0x1C0000
#define FTM 0x1D0000

#define R5_ROM 0x3E0000
#define R5_0_DEBUG 0x3F0000
#define R5_1_DEBUG 0x3F2000
#define R5_0_CTI 0x3F8000
#define R5_1_CTI 0x3F9000
#define R5_0_ETM 0x3FC000
#define R5_1_ETM 0x3FD000

#define A53_ROM 0x400000
#define A53_0_DEBUG 0x410000
#define A53_0_CTI 0x420000
#define A53_0_PMU 0x430000
#define A53_0_ETM 0x440000
#define A53_1_DEBUG 0x510000
#define A53_1_CTI 0x520000
#define A53_1_PMU 0x530000
#define A53_1_ETM 0x540000
#define A53_2_DEBUG 0x610000
#define A53_2_CTI 0x620000
#define A53_2_PMU 0x630000
#define A53_2_ETM 0x640000
#define A53_3_DEBUG 0x710000
#define A53_3_CTI 0x720000
#define A53_3_PMU 0x730000
#define A53_3_ETM 0x740000

/* Component offset group end */

/* Trace Memory Controller (TMC) register group  start */

#define TMCTRG 0x01C
#define FFCR 0x304
#define CTL 0x20

/* Trace Memory Controller (TMC) register group  end */

/* ETM register group start */

#define TRCPRGCTLR		0x004
#define TRCSTATR		0x00c

#define TRCLAR			0xfb0
#define TRCLSR			0xfb4
#define TRCOSLAR		0x300
#define TRCOSLSR		0x304

#define TRCCONFIGR		0x010
#define TRCEVENTCTL0R 	0x020
#define TRCEVENTCTL1R 	0x024
#define TRCSTALLCTLR 	0x02c
#define TRCSYNCPR 		0x034
#define TRCTRACEIDR		0x040
#define TRCTSCTLR 		0x030
#define TRCVICTLR 		0x080
#define TRCVIIECTLR 	0x084
#define TRCVISSCTLR		0x088
#define TRCCCCTLR       0x038
#define TRCEXTINSELR    0x120
#define TRCRSCTLR0      0x200
#define TRCRSCTLR1      0x204
#define TRCRSCTLR2      0x208
#define TRCRSCTLR3      0x20c
#define TRCRSCTLR4      0x210
#define TRCRSCTLR5      0x214
#define TRCRSCTLR6      0x218
#define TRCRSCTLR7      0x21c
#define TRCRSCTLR8      0x220
#define TRCRSCTLR9      0x224
#define TRCRSCTLR10     0x228
#define TRCRSCTLR11     0x22c
#define TRCRSCTLR12     0x230
#define TRCRSCTLR13     0x234
#define TRCRSCTLR14     0x238
#define TRCRSCTLR15     0x23c

// Address Comparator Value Register
#define TRCACVR0  0x400
#define TRCACVR1  0x408
#define TRCACVR2  0x410
#define TRCACVR3  0x418
#define TRCACVR4  0x420
#define TRCACVR5  0x428
#define TRCACVR6  0x430
#define TRCACVR7  0x438
#define TRCACVR8  0x440
#define TRCACVR9  0x448
#define TRCACVR10   0x450
#define TRCACVR11   0x458
#define TRCACVR12   0x460
#define TRCACVR13   0x468
#define TRCACVR14   0x470
#define TRCACVR15   0x478

// Address Comparator Value Register
#define TRCACATR0  0x480
#define TRCACATR1  0x488
#define TRCACATR2  0x490
#define TRCACATR3  0x498
#define TRCACATR4  0x4a0
#define TRCACATR5  0x4a8
#define TRCACATR6  0x4b0
#define TRCACATR7  0x4b8
#define TRCACATR8  0x4c0
#define TRCACATR9  0x4c8
#define TRCACATR10   0x4d0
#define TRCACATR11   0x4d8
#define TRCACATR12   0x4e0
#define TRCACATR13   0x4e8
#define TRCACATR14   0x4f0
#define TRCACATR15   0x4f8

// Context ID comparator, Cortex-A53 has only one
#define TRCCIDCVRD0 0x600
#define TRCCIDCCTLR 0x680

/* ETM register group END */


/*  
ETM functional group 

Usage:
    when a milestone hit event happens, performce the following procedure
    1. etm_disable(id)
    2. update address comparator pair to monitor the next set of milestones, 
       by calling etm_write_acvr_pair(id, ac_id, addr_val)
       the ac_id ranges from 0 to 3 inclusive
    3. etm_enable(id)
*/

void etm_enable(uint8_t id);
void etm_disable(uint8_t id);
void etm_write_acvr_pair(uint8_t id, uint8_t ac_id, uint32_t addr_val);
void etr_disable(void);
void etr_enable(void);





#endif
