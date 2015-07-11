#include <userland.h>
#include <types.h>

#define SECTION_START __attribute__((section(".text.start")))

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* g_terminal = (uint16_t*)0xB8000;

enum vga_color
{
    vga_color_black         = 0,
    vga_color_blue          = 1,
    vga_color_green         = 2,
    vga_color_cyan          = 3,
    vga_color_red           = 4,
    vga_color_magenta       = 5,
    vga_color_brown         = 6,
    vga_color_light_grey    = 7,
    vga_color_dark_grey     = 8,
    vga_color_light_blue    = 9,
    vga_color_light_green   = 10,
    vga_color_light_cyan    = 11,
    vga_color_light_red     = 12,
    vga_color_light_magenta = 13,
    vga_color_light_brown   = 14,
    vga_color_white         = 15,
};

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

SECTION_START int main()
{
    screen_put_entry(screen_create_entry('H', vga_color_light_blue), 0, 10);
    screen_put_entry(screen_create_entry('E', vga_color_light_blue), 1, 10);
    screen_put_entry(screen_create_entry('L', vga_color_light_blue), 2, 10);
    screen_put_entry(screen_create_entry('L', vga_color_light_blue), 3, 10);
    screen_put_entry(screen_create_entry('O', vga_color_light_blue), 4, 10);

    return 1;
}

