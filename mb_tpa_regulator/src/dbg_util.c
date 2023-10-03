#include "dbg_util.h"
#include "xil_printf.h"

void report(const char* format, ... ) {
	va_list args;
	xil_printf("REGULATOR: ");
	va_start(args, format);
	xil_vprintf(format, args);
	va_end(args);
	xil_printf("\n\r");
}
