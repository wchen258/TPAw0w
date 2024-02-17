#include "log_util.h"
#include <string.h>

volatile struct logger_t __attribute__((section(".log_mem_zone"))) records_log = {0};

static uint32_t next_ms_id[4];

void add_record(uint32_t core_id, struct ms_record record) {
	records_log.records[core_id][next_ms_id[core_id]++] = record;
}

void reset_log_util() {
    next_ms_id[0] = 0;
    next_ms_id[1] = 0;
    next_ms_id[2] = 0;
    next_ms_id[3] = 0;
    memset((void*) &records_log, 0, sizeof(records_log));
}
