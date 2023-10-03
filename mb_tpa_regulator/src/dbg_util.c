#include "dbg_util.h"
#include "xil_printf.h"


volatile uint32_t debug_flag = 0;
uint32_t debug_count1[4];
uint32_t debug_count2[4];
uint32_t debug_count3[4];
volatile struct debugger __attribute__((section(".dbg_mem_zone"))) dbg = {0};

void report(const char* format, ... ) {
	va_list args;
	xil_printf("REGULATOR: ");
	va_start(args, format);
	xil_vprintf(format, args);
	va_end(args);
	xil_printf("\n\r");
}


void debug_ptr(uint32_t point) {
	debug_flag = debug_flag | (0x1 << point);
}
