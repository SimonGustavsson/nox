#ifndef NOX_TERMINAL_H
#define NOX_TERMINAL_H

void terminal_init();
void terminal_reset_color();
void terminal_set_color(enum vga_color fg, enum vga_color bg);
void terminal_write_string(const char* data);
void terminal_write_char(const char c);
void terminal_write_uint32(uint32_t val);
void terminal_write_hex(uint32_t val);

#endif
