#if !defined(__cplusplus)
#include <stdbool.h> // C Does not have bool :(
#endif

#include "string.h"
#include "stddef.h"
#include "stdint.h"
#include "terminal.h"

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
 
static size_t g_current_row;
static size_t g_current_column;
static uint8_t g_current_color;
static uint16_t* g_buffer;

// -------------------------------------------------------------------------
// Forward declaractions
// -------------------------------------------------------------------------
static uint16_t vgaentry_create(char c, uint8_t color);
static void terminal_put_entry_at(char c, uint8_t color, size_t x, size_t y);
static void terminal_put_char(char c);

// -------------------------------------------------------------------------
// Externs
// -------------------------------------------------------------------------

uint8_t terminal_create_color(vga_color fg, vga_color bg)
{
	return fg | bg << 4;
}

void terminal_init()
{
	g_current_row = 0;
	g_current_column = 0;
	g_current_color = terminal_create_color(vga_color_light_grey, vga_color_black);
	g_buffer = (uint16_t*) 0xB8000;
	for ( size_t y = 0; y < VGA_HEIGHT; y++ )
	{
		for ( size_t x = 0; x < VGA_WIDTH; x++ )
		{
			const size_t index = y * VGA_WIDTH + x;
			g_buffer[index] = vgaentry_create(' ', g_current_color);
		}
	}
}

void terminal_set_color(uint8_t color)
{
	g_current_color = color;
}


void terminal_write_string(const char* data)
{
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++)
		terminal_put_char(data[i]);
}

void terminal_write_uint32(uint32_t val)
{
	char buf[9];
	itoa(val, buf);
	terminal_write_string(buf);
}

void terminal_write_hex_byte(uint8_t val)
{
	terminal_put_char(nybble_to_ascii((val >> 4) & 0xF));
	terminal_put_char(nybble_to_ascii((val >> 0) & 0xF));
}

void terminal_write_hex(uint32_t val)
{
	terminal_put_char('0');
	terminal_put_char('x');
	terminal_write_hex_byte((val >> 24) & 0xFF);
	terminal_write_hex_byte((val >> 16) & 0xFF);
	terminal_write_hex_byte((val >> 8) & 0xFF);
	terminal_write_hex_byte((val >> 0) & 0xFF);
}

// -------------------------------------------------------------------------
// Private
// -------------------------------------------------------------------------
static uint16_t vgaentry_create(char c, uint8_t color)
{
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

static void terminal_put_entry_at(char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	g_buffer[index] = vgaentry_create(c, color);
}

static void terminal_put_char(char c)
{
	if(c == '\n')
	{
		g_current_row++;
	    g_current_column = 0;
		return;
	}

	terminal_put_entry_at(c, g_current_color, g_current_column, g_current_row);
	if (++g_current_column == VGA_WIDTH)
	{
		g_current_column = 0;
		if (++g_current_row == VGA_HEIGHT)
		{
			g_current_row = 0;
		}
	}
}

