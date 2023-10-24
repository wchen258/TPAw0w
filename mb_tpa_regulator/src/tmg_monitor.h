#ifndef TMG_MONITOR_H
#define TMG_MONITOR_H

#include "common.h"

void report_address_hit(uint8_t id, uint32_t address);
void handle_hit(uint8_t);
void reset_tmg_buf(void);
void report_tmg_mem(void);

#endif // TMG_MONITOR_H

