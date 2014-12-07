#ifndef TERMINAL_H
#define TERMINAL_H

#include "stdint.h"

typedef enum 
{
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
} VgaColor;

void terminal_initialize();
uint8_t terminal_createColor(VgaColor fg, VgaColor bg);
void terminal_setColor(uint8_t color);
void terminal_putChar(char c);
void terminal_writeString(const char* data);
void terminal_writeUint32(uint32_t val);
void terminal_writeHex(uint32_t val);

#endif
