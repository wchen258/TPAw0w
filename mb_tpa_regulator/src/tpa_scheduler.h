#ifndef TPA_SCHEDULER_H
#define TPA_SCHEDULER_H

#include "log_util.h"

void reset_sched(void);

void invoke_sched_vanilla(uint8_t id, uint32_t real_time, uint32_t* acc_nominal_time, struct ms_record* log_ms_record);
void invoke_sched_epilogue_vanilla(uint8_t id, struct ms_record* log_ms_record);

void invoke_sched_vanilla_2lvl(uint8_t id, uint32_t real_time, uint32_t* acc_nominal_time, struct ms_record* log_ms_record);
void invoke_sched_epilogue_vanilla_2lvl(uint8_t id, struct ms_record* log_ms_record);

#endif // TPA_SCHEDULER_H

