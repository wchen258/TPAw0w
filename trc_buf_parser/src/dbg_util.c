#include "dbg_util.h"
#include "xil_printf.h"

volatile struct debugger __attribute__((section(".dbg_mem_zone"))) dbg = {0};
volatile struct inter_rpu * inter_rpu_com = (volatile struct inter_rpu *)(R0_BTCM_BASE + 0x8410);

void reset_dbg_util() {
    memset(&dbg, 0, sizeof(dbg));
}

void reset_inter_rpu_com(void) {
    memset(&inter_rpu_com, 0, sizeof(inter_rpu_com));
}