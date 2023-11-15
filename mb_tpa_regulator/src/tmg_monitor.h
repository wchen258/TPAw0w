#ifndef TMG_MONITOR_H
#define TMG_MONITOR_H

#include "common.h"

void report_event_hit(uint8_t, uint8_t);
void report_address_hit(uint8_t, uint32_t);
void handle_hit(uint8_t);
void reset_tmg_buf(void);
void report_tmg_mem(void);

#endif // TMG_MONITOR_H

