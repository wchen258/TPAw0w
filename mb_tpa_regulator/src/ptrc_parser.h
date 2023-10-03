#ifndef PTRC_PARSER_H
#define PTRC_PARSER_H

#include "common.h"

void handle_buffer(uint8_t);
void apply_virtual_offset();
void reset_ptrc_buf(uint8_t);
void report_ptrc_mem();

#endif // PTRC_PARSER_H