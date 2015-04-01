#ifndef TERMINAL_H
#define TERMINAL_H

#include "stdint.h"

typedef enum 
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
} vga_color;

void terminal_init();
uint8_t terminal_create_color(vga_color fg, vga_color bg);
void terminal_set_color(uint8_t color);
void terminal_put_char(char c);
void terminal_write_string(const char* data);
void terminal_write_uint32(uint32_t val);
void terminal_write_hex(uint32_t val);

#endif
