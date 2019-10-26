#include <types.h>
#include <screen.h>
#include <terminal.h>
#include <string.h>
#include <bit_utils.h>
#include <pio.h>

#define BochsConsolePrintChar(c) OUTB(0xe9, c)

// -------------------------------------------------------------------------
// Static defines
// -------------------------------------------------------------------------
#define BUFFER_MAX_ROWS (50)
#define BUFFER_MAX_COLUMNS (80)
#define BUFFER_MAX_OFFSET (25)
#define TERMINAL_INDENTATION_LENGTH 4
#define SPACES_PER_TAB (4)

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
static size_t g_current_row;
static size_t g_current_column;
static uint8_t g_current_color;
static uint32_t g_current_indentation;

// TODO: This kinda goes against our conventions of large
//       objects in the data region, but we don't support
//       dynamic allocations yet. :-(
static uint16_t g_buffer[BUFFER_MAX_ROWS][BUFFER_MAX_COLUMNS];

// Indicates how far down the buffer we have scrolled
static size_t g_buffer_row_offset = 0;

// -------------------------------------------------------------------------
// Forward declaractions
// -------------------------------------------------------------------------
static uint16_t vgaentry_create(char c, uint8_t color);
static void buffer_sync_with_screen();

void terminal_init()
{
	g_current_row = 0;
	g_current_column = 0;
    g_current_indentation = 0;
    terminal_reset_color();
}

void terminal_clear()
{
    uint16_t empty = vgaentry_create(' ', g_current_color);

    for(size_t x = 0; x < BUFFER_MAX_COLUMNS; x++) {
        for(size_t y = 0; y < BUFFER_MAX_ROWS; y++) {
            g_buffer[y][x] = empty;
        }
    }

    g_current_row = 0;
    g_current_column = 0;
    g_buffer_row_offset = 0;

    buffer_sync_with_screen();
}

void terminal_reset_color()
{
    terminal_set_color(vga_color_light_grey, vga_color_black);
}

void terminal_set_color(enum vga_color fg, enum vga_color bg)
{
	g_current_color = screen_create_color(fg, bg);
}

void terminal_write_string_endpadded(const char* data, size_t total_length)
{
    size_t str_len = strlen(data);
    size_t spaces_to_insert = total_length - str_len;

    terminal_write_string_n(data, str_len);

    for(size_t i = 0; i < spaces_to_insert; i++) {
        terminal_write_char(' ');
    }
}

void terminal_write_string(const char* data)
{
	size_t data_len = strlen(data);

    terminal_write_string_n(data, data_len);
}

void terminal_write_string_n(const char* data, size_t length)
{
	for (size_t i = 0; i < length; i++)
		terminal_write_char(data[i]);
}

void terminal_write_uint32(uint32_t val)
{
	char buf[11];
	uitoa(val, buf);
	terminal_write_string(buf);
}

void terminal_write_hex_byte(uint8_t val)
{
	terminal_write_char(nybble_to_ascii((val >> 4) & 0xF));
	terminal_write_char(nybble_to_ascii((val >> 0) & 0xF));
}

void terminal_write_ptr(void* val)
{
#if PLATFORM_BITS==32
    terminal_write_uint32_x((uint32_t)val);
#elif PLATFORM_BITS==64
    terminal_write_uint64_x((uint64_t)val);
#else
#   error PLATFORM_BITS has an unsupported value.
#endif
}

void terminal_write_uint8_x(uint8_t val)
{
    terminal_write_char('0');
    terminal_write_char('x');
    terminal_write_hex_byte(val);
}

void terminal_write_uint16_x(uint16_t val)
{
    terminal_write_char('0');
    terminal_write_char('x');

    uint8_t byte1 = ((val & 0xFF00) >> 8);
    uint8_t byte2 = ((val & 0xFF));
    terminal_write_hex_byte(byte1);
    terminal_write_hex_byte(byte2);
}

void terminal_write_uint24_x(uint32_t val)
{
    terminal_write_char('0');
    terminal_write_char('x');
	terminal_write_hex_byte(BYTE(val, 2));
	terminal_write_hex_byte(BYTE(val, 1));
	terminal_write_hex_byte(BYTE(val, 0));
}

void terminal_write_uint32_x(uint32_t val)
{
	terminal_write_char('0');
	terminal_write_char('x');
	terminal_write_hex_byte(BYTE(val, 3));
	terminal_write_hex_byte(BYTE(val, 2));
	terminal_write_hex_byte(BYTE(val, 1));
	terminal_write_hex_byte(BYTE(val, 0));
}

void terminal_write_uint64_x(uint64_t val)
{
	terminal_write_char('0');
	terminal_write_char('x');
	terminal_write_hex_byte(BYTE(val, 7));
	terminal_write_hex_byte(BYTE(val, 6));
	terminal_write_hex_byte(BYTE(val, 5));
	terminal_write_hex_byte(BYTE(val, 4));
	terminal_write_hex_byte(BYTE(val, 3));
	terminal_write_hex_byte(BYTE(val, 2));
	terminal_write_hex_byte(BYTE(val, 1));
	terminal_write_hex_byte(BYTE(val, 0));
}

void terminal_write_uint64_bytes(uint64_t val)
{
	terminal_write_hex_byte(BYTE(val, 0));
	terminal_write_hex_byte(BYTE(val, 1));
	terminal_write_hex_byte(BYTE(val, 2));
	terminal_write_hex_byte(BYTE(val, 3));
	terminal_write_hex_byte(BYTE(val, 4));
	terminal_write_hex_byte(BYTE(val, 5));
	terminal_write_hex_byte(BYTE(val, 6));
	terminal_write_hex_byte(BYTE(val, 7));
}

static void buffer_sync_with_screen()
{
    for(size_t x = 0; x < screen_width_get(); x++) {
        for(size_t y = 0; y < screen_height_get(); y++) {
           screen_put_entry(g_buffer[g_buffer_row_offset + y][x], x, y);
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

    // And clear the last row
    for(size_t i = 0; i < BUFFER_MAX_COLUMNS; i++) {
        g_buffer[BUFFER_MAX_ROWS - 1][i] = ' ';
    }

    buffer_sync_with_screen();
}

static void buffer_increase_offset()
{
    if(g_buffer_row_offset < BUFFER_MAX_OFFSET) {
        g_buffer_row_offset++;
    }

    // Buffer has moved up a row, we now need to resync the terminal
    // to clear the bottom row
    buffer_sync_with_screen();
}

bool terminal_erase_char_last()
{
    if(g_current_column > 0) {
        // Easy, just step back a column!
        g_current_column--;
    }
    else {
        if(g_current_row <= 0)
            return false;

        g_current_row--;
        g_current_column = BUFFER_MAX_COLUMNS - 1;
    }

    g_buffer[g_current_row][g_current_column] = ' ';

    buffer_sync_with_screen();

    return true;
}

void terminal_write_char(const char c)
{
    if(c == '\t') {
        for(int i = 0; i < SPACES_PER_TAB; i++) {
            terminal_write_char(' ');
        }
        return;
    }

    // OK - so first off, can we write to the current location,
    // or do we need to perform some operations on the buffer?
    if(g_current_column >= BUFFER_MAX_COLUMNS) {
        // Our last write wrote to the last columns, hop to next row
        // (the row will be verified next and clamped if need be)
        g_current_row++;
        g_current_column = g_current_indentation;

        BochsConsolePrintChar('\n');
    }

    if(g_current_row >= screen_height_get() + g_buffer_row_offset) {

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
        g_current_row = screen_height_get() + g_buffer_row_offset - 1;
    }

    if(c == '\n') {
        g_current_row++;
        g_current_column = g_current_indentation;
        BochsConsolePrintChar('\n');
        return;
    }

    // Write the character to our buffer
    g_buffer[g_current_row][g_current_column] = vgaentry_create(c, g_current_color);
    BochsConsolePrintChar(c);

    if (g_current_row >= g_buffer_row_offset) {
        // This also needs writing to the screen
        screen_put_char(c, g_current_color, g_current_column, g_current_row - g_buffer_row_offset);
    }

    if (++g_current_column == BUFFER_MAX_COLUMNS)
    {
        g_current_column = g_current_indentation;
        if (++g_current_row - g_buffer_row_offset > screen_height_get())
        {
            g_current_row = 0;
        }
    }
}

void terminal_indentation_increase()
{
    if(g_current_indentation < BUFFER_MAX_COLUMNS - 1) {

        bool update_column = g_current_column == g_current_indentation;

        g_current_indentation += TERMINAL_INDENTATION_LENGTH;

        if(update_column) {
            g_current_column = g_current_indentation;
        }
    }
}

void terminal_indentation_decrease()
{
    if(g_current_indentation >= TERMINAL_INDENTATION_LENGTH) {

        bool update_column = g_current_column == g_current_indentation;

        g_current_indentation -= TERMINAL_INDENTATION_LENGTH;

        if(update_column) {
            g_current_column = g_current_indentation;
        }
    }
}

static uint16_t vgaentry_create(char c, uint8_t color)
{
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

