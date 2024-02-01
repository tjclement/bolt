#ifndef __GLITCH_H__
#define __GLITCH_H__

#include <stdint.h>

void glitch_init();
void glitch_offset(uint32_t offset);
void glitch_length(uint32_t length);
void glitch_trigger();

#endif