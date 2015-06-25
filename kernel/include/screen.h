#ifndef NOX_SCREEN_H
#define NOX_SCREEN_H

#include <types.h>
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

typedef uint8_t screen_color;

void screen_init();
void screen_cursor_hide();
screen_color screen_create_color(enum vga_color foreground, enum vga_color background);
void screen_put_char(char c, screen_color color, size_t x, size_t y);
void screen_put_entry(uint16_t entry, size_t x, size_t y);
uint16_t screen_create_entry(char c, enum vga_color color);
uint32_t screen_width_get();
uint32_t screen_height_get();

#endif
