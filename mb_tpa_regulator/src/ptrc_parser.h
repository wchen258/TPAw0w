#ifndef PTRC_PARSER_H
#define PTRC_PARSER_H

#include "common.h"

void handle_buffer(uint8_t);
void apply_virtual_offset(void);
void reset_ptrc(void);
void report_ptrc_mem(void);

#endif // PTRC_PARSER_H