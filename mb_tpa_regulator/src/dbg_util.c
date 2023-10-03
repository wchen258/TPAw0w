#include "dbg_util.h"
#include "xil_printf.h"

volatile struct debugger __attribute__((section(".dbg_mem_zone"))) dbg = {0};

void report(const char* format, ... ) {
	va_list args;
	xil_printf("REGULATOR: ");
	va_start(args, format);
	xil_vprintf(format, args);
	va_end(args);
	xil_printf("\n\r");
}