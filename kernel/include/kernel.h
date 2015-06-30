#ifndef NOX_KERNEL_H
#define NOX_KERNEL_H

#define PACKED __attribute__((__packed__))
#define SECTION_BOOT __attribute__((section(".text.boot")))
#define NO_INLINE __attribute__((noinline))

#define PRINTK(str, fg, bg) \
            do {                                 \
                terminal_set_color(fg, bg);      \
                terminal_write_string(str "\n"); \
                terminal_reset_color();          \
            } while(0)

#define KINFO(str) PRINTK(str, vga_color_light_grey, vga_color_black)
#define KWARN(str) PRINTK(str, vga_color_light_red, vga_color_black)
#define KERROR(str) PRINTK(str, vga_color_red, vga_color_black)
#define KPANIC(str)                   \
    do {                              \
        KERROR(str);                  \
        while(1);                     \
    } while(0);
#define SHOWVAL(str, val)             \
    do {                              \
        terminal_write_string(str);   \
        terminal_write_uint32(val);   \
        terminal_write_char('\n');    \
    } while(0)

#define SHOWVAL_x(str, val)           \
    do {                              \
        terminal_write_string(str);   \
        terminal_write_uint32_x(val); \
        terminal_write_char('\n');    \
    } while(0)
#define SHOWSTR(str, val)           \
    do {                              \
        terminal_write_string(str);   \
        terminal_write_string(val); \
        terminal_write_char('\n');    \
    } while(0)

enum kresult
{
    kresult_ok = 0,
    kresult_invalid = -1
};

#endif
