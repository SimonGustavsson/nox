#include <types.h>
#include <string.h>
#include <screen.h>
#include <pio.h>

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* g_terminal;

// -------------------------------------------------------------------------
// Forward declaractions
// -------------------------------------------------------------------------
uint8_t screen_create_color(enum vga_color fg, enum vga_color bg);
void screen_put_entry(uint16_t entry, size_t x, size_t y);
void screen_put_char(char c, uint8_t color, size_t x, size_t y);

// -------------------------------------------------------------------------
// Externs
// -------------------------------------------------------------------------

void screen_init()
{
	g_terminal = (uint16_t*) 0xB8000;

    // Clear out the terminal
	for ( size_t y = 0; y < VGA_HEIGHT; y++ )
	{
		for ( size_t x = 0; x < VGA_WIDTH; x++ )
		{
			const size_t index = y * VGA_WIDTH + x;
			g_terminal[index] = screen_create_color(' ', vga_color_black);
		}
	}
}

void screen_cursor_hide()
{
    // Tell the VGA text mode
    // THING that we want to access the cursor start register
    // which is at index 0xA
    OUTB(0x3D4, 0xA);
    uint8_t val = INB(0x3D5);

    val |= (1 << 5);

    OUTB(0x3D5, val);
}

uint32_t screen_width_get()
{
    return VGA_WIDTH;
}
uint32_t screen_height_get()
{
    return VGA_HEIGHT;
}

// -------------------------------------------------------------------------
// Private
// -------------------------------------------------------------------------
uint16_t screen_create_entry(char c, enum vga_color color)
{
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | (color16 << 8);
}

void screen_put_entry(uint16_t entry, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    g_terminal[index] = entry;
}

void screen_put_char(char c, screen_color color, size_t x, size_t y)
{
    screen_put_entry(screen_create_entry(c, color), x, y);
}

screen_color screen_create_color(enum vga_color fg, enum vga_color bg)
{
	return fg | bg << 4;
}

