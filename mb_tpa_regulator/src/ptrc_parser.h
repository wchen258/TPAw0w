#ifndef PTRC_PARSER_H
#define PTRC_PARSER_H

#include "common.h"

extern volatile struct debugger dbg;
extern uint32_t virtual_offset ;
extern uint8_t virtual_read_failed ;
extern uint32_t current_buffer_id ;
extern volatile uint32_t debug_flag;
extern uint8_t in_range[4];
extern uint32_t debug_count1[4];
extern uint32_t debug_count2[4];
extern uint32_t debug_count3[4];
extern volatile uint8_t ptrc_buf[4][PTRC_BUFFER_SIZE / 4] ;
extern volatile uint32_t ptrc_abs_wpt[4] ;
extern volatile uint32_t ptrc_abs_rpt[4] ;

void debug_ptr(uint32_t point);
void handle_buffer(void);
void apply_virtual_offset();

#endif