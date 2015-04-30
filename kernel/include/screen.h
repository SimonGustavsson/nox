#ifndef NOX_TERMINAL_H
#define NOX_TERMINAL_H

#include "stdint.h"

// Concepts
//  - Screen - data rendering, NOT input
//  - Keyboard - low level keyboard handling, delivers to the active terminal
//  - Terminal - a thing that ties input and output together, visual or text
//  - Terminal Manager - Manages which terminal is currently active
//  - Interpreter - reads input and turns it into something the terminal understands

// AWOGA! PSEUDO CODE AHEAD!
// struct terminal {
//
//     // Fur return commands - returns some sort of value
//     kresult put_strn(char* chars, uint32_t len, void** response),
//     kresult put_strz(char* chars, void** response);
//     kresult activate(); // Start rendering
//     kresult deactive(); // Stop rendering etc...
//     kresult *receive(key);
// };
//
// struct text_screen {
//     init();
//     destroy();
//    screen_put_char(x,y,c);
// };
//
// struct visual_screen {
//     init();
//     destroy();
//     screen_put_pixel(x,y,p);
// };

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
void terminal_reset_color();
void terminal_set_color(vga_color fg, vga_color bg);
void terminal_write_string(const char* data);
void terminal_write_char(const char c);
void terminal_write_uint32(uint32_t val);
void terminal_write_hex(uint32_t val);

#endif
