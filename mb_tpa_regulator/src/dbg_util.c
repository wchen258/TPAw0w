#include "dbg_util.h"
#include "xil_printf.h"

volatile struct debugger __attribute__((section(".dbg_mem_zone"))) dbg = {0};

static uint32_t log_ptr = 0;

void reset_dbg_util() {
    log_ptr = 0;
    memset(&dbg, 0, sizeof(dbg));
}

void report(const char* format, ... ) {
	va_list args;
	xil_printf("REGULATOR: ");
	va_start(args, format);
	xil_vprintf(format, args);
	va_end(args);
	xil_printf("\n\r");
}

static inline void write_to_log(char c) {
	if (log_ptr < 1023)
    ;
		// dbg.log[log_ptr++] = c;
}

void breport(const char* format, ...) {
	uint32_t i, base, digit, argument;
	va_list args;

	i = 0;

	va_start(args, format);

	while (format && format[i]) {
		if (format[i] == '%') {
			i++;

			base = (format[i++] == 'd') ? 10 : 16;

			argument = va_arg(args, uint32_t);

			do {
				digit = (argument % base);

				if (base == 16 && digit >= 10) {
					write_to_log('A' + (digit - 10));
				} else {
					write_to_log('0' + digit);
				}

				argument /= base;
			} while (argument != 0);
		} else {
			write_to_log(format[i++]);
		}
	}

	write_to_log('\n');

	va_end(args);
}
