#if !defined(__cplusplus)
#include <stdbool.h> // C Does not have bool :(
#endif

#include "stddef.h"
#include "stdint.h"
#include "terminal.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
 
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

uint8_t terminal_createColor(VgaColor fg, VgaColor bg)
{
	return fg | bg << 4;
}

uint16_t vgaentry_create(char c, uint8_t color)
{
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

size_t strlen(const char* str)
{
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}

void terminal_initialize()
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = terminal_createColor(COLOR_LIGHT_GREY, COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	for ( size_t y = 0; y < VGA_HEIGHT; y++ )
	{
		for ( size_t x = 0; x < VGA_WIDTH; x++ )
		{
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vgaentry_create(' ', terminal_color);
		}
	}
}

void terminal_setColor(uint8_t color)
{
	terminal_color = color;
}

void terminal_putEntryAt(char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vgaentry_create(c, color);
}

void terminal_putChar(char c)
{
	if(c == '\n')
	{
		terminal_row++;
		terminal_column = 0;
		return;
	}

	terminal_putEntryAt(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH)
	{
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
		{
			terminal_row = 0;
		}
	}
}

void terminal_writeString(const char* data)
{
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++)
		terminal_putChar(data[i]);
}

void itoa(int number, char* buf)
{
	// We populate the string backwards, increment to make room for \0
	buf++;

	int negative = number < 0;
	if(negative)
	{
		buf++;
		number = -number;
	}

	// Find where our string will end
	int shifter = number;
	do
	{
		buf++;
		shifter /= 10;
	}while(shifter > 0);

	// Make sure the string is terminated nicely
	*--buf = '\0';
	
	// Start converting the digits into characters
	do
	{
		*--buf = '0' + (number % 10); // Muahaha!
		number /= 10;
	}while(number > 0);

	if(negative)
		*--buf = '-';
}

void terminal_writeUint32(uint32_t val)
{
	char buf[9];
	itoa(val, buf);
	terminal_writeString(buf);
}

uint8_t nybbleToAscii(uint8_t val)
{
	if (val < 0x0A)
		return '0' + val;
	else
		return 'A' + (val - 10);
}

void terminal_writeHexByte(uint8_t val)
{
	terminal_putChar(nybbleToAscii((val >> 4) & 0xF));
	terminal_putChar(nybbleToAscii((val >> 0) & 0xF));
}

void terminal_writeHex(uint32_t val)
{
	terminal_putChar('0');
	terminal_putChar('x');
	terminal_writeHexByte((val >> 24) & 0xFF);
	terminal_writeHexByte((val >> 16) & 0xFF);
	terminal_writeHexByte((val >> 8) & 0xFF);
	terminal_writeHexByte((val >> 0) & 0xFF);
}
