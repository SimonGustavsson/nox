// Kernel::main
// First line of C code to run
#include "stdint.h"

uint16_t make_vgaentry(char c, uint8_t attr)
{
	uint16_t c16 = c;
	uint16_t attr16 = attr;
	return c16 | attr16 << 8;
}

void _start()
{
	uint16_t* fb = (uint16_t*)0xB8000;
	fb[0] = make_vgaentry('h', 0x07);

	while(1);
}