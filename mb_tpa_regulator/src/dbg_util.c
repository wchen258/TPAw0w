#include <stdatomic.h>
#include "dbg_util.h"
#include "xil_printf.h"

volatile struct debugger __attribute__((section(".dbg_mem_zone"))) dbg = {0};

volatile struct inter_rpu * inter_rpu_com = (volatile struct inter_rpu *)(R0_BTCM_BASE + 0x8410);

static uint32_t log_ptr = 0;

void reset_dbg_util() {
    log_ptr = 0;
    memset(&dbg, 0, sizeof(dbg));
}

void report(const char* format, ... ) {
	//while (atomic_flag_test_and_set_explicit(&inter_rpu_com->report_lock, memory_order_seq_cst));

	va_list args;
	xil_printf("REGULATOR: ");
	va_start(args, format);
	xil_vprintf(format, args);
	va_end(args);
	xil_printf("\n\r");

	//sleep(1);

	//atomic_flag_clear_explicit(&inter_rpu_com->report_lock, memory_order_seq_cst);
}

static inline void write_to_log(char c) {
	/*
	if (log_ptr < 1023)
		dbg.log[log_ptr++] = c;
		*/
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
