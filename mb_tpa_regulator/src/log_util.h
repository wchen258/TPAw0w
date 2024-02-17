#ifndef LOG_UTIL_H
#define LOG_UTIL_H

#include <stdint.h>
#include <stdatomic.h>
#include "xtime_l.h"

#define R0_BTCM_BASE 0xFFE20000

struct ms_record {
    uint32_t address_parent;
    uint32_t address_child;
    uint32_t timestamp;
    uint32_t decision;
};

struct logger_t {
    struct ms_record records[4][150];
};

extern volatile struct logger_t records_log;

void add_record(uint32_t core_id, struct ms_record record);
void reset_log_util();

#endif // LOG_UTIL_H
