#include <stdbool.h> // C Does not have bool :(
#include "string.h"
#include "stddef.h"
#include "stdint.h"
#include "terminal.h"

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
#define BUFFER_MAX_ROWS (50)
#define BUFFER_MAX_COLUMNS (80)
#define BUFFER_MAX_OFFSET (25)

static size_t g_current_row;
static size_t g_current_column;
static uint8_t g_current_color;
static uint16_t* g_terminal;

// TODO: This kinda goes against our conventions of large
//       objects in the data region, but we don't support
//       dynamic allocations yet. :-(
static uint16_t g_buffer[BUFFER_MAX_ROWS][BUFFER_MAX_COLUMNS];

// Indicates how far down the buffer we have scrolled
static size_t g_buffer_row_offset = 0;

// -------------------------------------------------------------------------
// Forward declaractions
// -------------------------------------------------------------------------
static uint8_t terminal_create_color(vga_color fg, vga_color bg);
static uint16_t vgaentry_create(char c, uint8_t color);
static void terminal_put_colored_entry_at(uint16_t entry, size_t x, size_t y);
static void terminal_put_entry_at(char c, uint8_t color, size_t x, size_t y);

// -------------------------------------------------------------------------
// Externs
// -------------------------------------------------------------------------

void terminal_init()
{
	g_current_row = 0;
	g_current_column = 0;
	g_terminal = (uint16_t*) 0xB8000;
    terminal_reset_color();

    // Clear out the terminal
	for ( size_t y = 0; y < VGA_HEIGHT; y++ )
	{
		for ( size_t x = 0; x < VGA_WIDTH; x++ )
		{
			const size_t index = y * VGA_WIDTH + x;
			g_terminal[index] = vgaentry_create(' ', g_current_color);
		}
	}

    // Empty the buffer
    for(size_t y = 0; y < BUFFER_MAX_ROWS; y++) {
        for(size_t x = 0; x < BUFFER_MAX_COLUMNS; x++) {
            g_buffer[y][x] = 0;
        }
    }
}

void terminal_reset_color()
{
    terminal_set_color(vga_color_light_grey, vga_color_black);
}

void terminal_set_color(vga_color fg, vga_color bg)
{
	g_current_color = terminal_create_color(fg, bg);
}

void terminal_write_string(const char* data)
{
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++)
		terminal_write_char(data[i]);
}

void terminal_write_uint32(uint32_t val)
{
	char buf[9];
	itoa(val, buf);
	terminal_write_string(buf);
}

void terminal_write_hex_byte(uint8_t val)
{
	terminal_write_char(nybble_to_ascii((val >> 4) & 0xF));
	terminal_write_char(nybble_to_ascii((val >> 0) & 0xF));
}

void terminal_write_hex(uint32_t val)
{
	terminal_write_char('0');
	terminal_write_char('x');
	terminal_write_hex_byte((val >> 24) & 0xFF);
	terminal_write_hex_byte((val >> 16) & 0xFF);
	terminal_write_hex_byte((val >> 8) & 0xFF);
	terminal_write_hex_byte((val >> 0) & 0xFF);
}

static void buffer_sync_with_terminal()
{
    for(size_t x = 0; x < VGA_WIDTH; x++) {
        for(size_t y = 0; y < VGA_HEIGHT - 1; y++) {
           terminal_put_colored_entry_at(g_buffer[g_buffer_row_offset + y][x], x, y);
        }
    }
}

static void buffer_scroll_up()
{
    for(size_t x = 1; x < BUFFER_MAX_ROWS; x++) {
        for(size_t y = 0; y < BUFFER_MAX_COLUMNS; y++) {
            g_buffer[x - 1][y] = g_buffer[x][y];
        }
    }

    buffer_sync_with_terminal();
}

static void buffer_increase_offset()
{
    g_buffer_row_offset++;

    if(g_buffer_row_offset > BUFFER_MAX_OFFSET) {
        while(1); // TODO: How to recover?
    }

    // Buffer has moved up a row, we now need to resync the terminal
    // to clear the bottom row
    buffer_sync_with_terminal();
}

void terminal_write_char(const char c)
{
    // OK - so first off, can we write to the current location,
    // or do we need to perform some operations on the buffer?
    if(g_current_column == BUFFER_MAX_COLUMNS) {
        // Our last write wrote to the last columns, hop to next row
        // (the row will be verified next and clamped if need be)
        g_current_row++;
        g_current_column = 0;
    }

    if(g_current_row >= VGA_HEIGHT + g_buffer_row_offset) {

        // We have gone past the screen

        if(g_buffer_row_offset < BUFFER_MAX_OFFSET) {

            // Buffer can scroll, so do it and sync up with the terminal
            buffer_increase_offset();
        }
        else {
            // Buffer has scrolled max amount - and we're trying to
            // write to the next row, shift everything in the buffer up one row
            // discarding the entries in the first row
            buffer_scroll_up();
        }

        // Set current row to the last row, again
        g_current_row = VGA_HEIGHT + g_buffer_row_offset - 1;
    }

    if(c == '\n') {
        g_current_row++;
        g_current_column = 0;
        return;
    }

    // Write the character to our buffer
    g_buffer[g_current_row][g_current_column] = vgaentry_create(c, g_current_color);

    if(g_current_row >= g_buffer_row_offset) {
        // This also needs writing to the screen
        terminal_put_entry_at(c, g_current_color, g_current_column, g_current_row - g_buffer_row_offset);
    }

	if (++g_current_column == BUFFER_MAX_COLUMNS)
	{
		g_current_column = 0;
		if (++g_current_row - g_buffer_row_offset > VGA_HEIGHT)
		{
			g_current_row = 0;
		}
	}
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

static void terminal_put_colored_entry_at(uint16_t entry, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    g_terminal[index] = entry;
}

static void terminal_put_entry_at(char c, uint8_t color, size_t x, size_t y)
{
    terminal_put_colored_entry_at(vgaentry_create(c, color), x, y);
}

static uint8_t terminal_create_color(vga_color fg, vga_color bg)
{
	return fg | bg << 4;
}

