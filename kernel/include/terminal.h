#ifndef NOX_TERMINAL_H
#define NOX_TERMINAL_H

#include <types.h>
#include <screen.h>

void terminal_init();
void terminal_clear();
void terminal_reset_color();
void terminal_set_color(enum vga_color fg, enum vga_color bg);
void terminal_write_string(const char* data);
void terminal_write_string_n(const char* data, size_t length);
void terminal_write_char(const char c);
void terminal_write_uint16_x(uint16_t val);
void terminal_write_uint32(uint32_t val);
void terminal_indentation_increase();
void terminal_indentation_decrease();
void terminal_write_uint32_x(uint32_t val);
void terminal_write_uint64_x(uint64_t val);
void terminal_write_ptr(void* val);

#endif
