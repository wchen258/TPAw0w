#include "dbg_util.h"
#include "xil_printf.h"

volatile struct debugger __attribute__((section(".dbg_mem_zone"))) dbg = {0};

void reset_dbg_util() {
    memset(&dbg, 0, sizeof(dbg));
}