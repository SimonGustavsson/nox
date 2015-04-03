#ifndef KERNEL_H
#define KERNEL_H

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

#endif
