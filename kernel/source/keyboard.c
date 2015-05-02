#include <types.h>
#include <stdbool.h>

#include <pic.h>
#include <pio.h>
#include <interrupt.h>
#include <kernel.h>
#include <keyboard.h>
#include <scan_code.h>
#include <terminal.h>
#include <debug.h>

enum key_event_type {
    key_event_type_down,
    key_event_type_up
};

struct kb_subscriber*   g_current_subscriber;
bool g_initial_ack_received;
struct sc_set*          g_current_set;
struct sc_map*          g_current_map;

static void kb_handle_interrupt(uint8_t irq, struct irq_regs* regs);

char* kb_get_special_key_name(enum keys key)
{
    switch(key) {
        case keys_backspace: return "backspace";
        case keys_tab: return "tab";
        case keys_capslock: return "caps-lock";
        case keys_lshift: return "l-shift";
        case keys_lctrl: return "l-ctrl";
        case keys_lgui: return "l-gui";
        case keys_lalt: return "l-alt";
        case keys_rshift: return "r-shift";
        case keys_rctrl: return "r-ctrl";
        case keys_rgui: return "r-gui";
        case keys_ralt: return "r-alt";
        case keys_enter: return "enter";
        case keys_escape: return "escape";
        case keys_insert: return "insert";
        case keys_home: return "home";
        case keys_pageup: return "page-up";
        case keys_pagedown: return "page-down";
        case keys_delete: return "delete";
        case keys_end: return "end";
        case keys_f1: return "f1";
        case keys_f2: return "f2";
        case keys_f3: return "f3";
        case keys_f4: return "f4";
        case keys_f5: return "f5";
        case keys_f6: return "f6";
        case keys_f7: return "f7";
        case keys_f8: return "f8";
        case keys_f9: return "f9";
        case keys_f10: return "f10";
        case keys_f11: return "f11";
        case keys_f12: return "f12";
        case keys_numlock: return "num-lock";
        case keys_num_enter: return "num-enter";
        case keys_left: return "left";
        case keys_up: return "up";
        case keys_right: return "right";
        case keys_down: return "down";
        default:
            return "<unknown>";
    }
}

char kb_key_to_ascii(enum keys key)
{
    switch(key) {
        case keys_a:  return 'a';
        case keys_b:  return 'b';
        case keys_c:  return 'c';
        case keys_d:  return 'd';
        case keys_e:  return 'e';
        case keys_f:  return 'f';
        case keys_g:  return 'g';
        case keys_h:  return 'h';
        case keys_i:  return 'i';
        case keys_j:  return 'j';
        case keys_k:  return 'k';
        case keys_l:  return 'l';
        case keys_m:  return 'm';
        case keys_n:  return 'n';
        case keys_o:  return 'o';
        case keys_p:  return 'p';
        case keys_q:  return 'q';
        case keys_r:  return 'r';
        case keys_s:  return 's';
        case keys_t:  return 't';
        case keys_u:  return 'u';
        case keys_v:  return 'v';
        case keys_w:  return 'w';
        case keys_x:  return 'x';
        case keys_y:  return 'y';
        case keys_z:  return 'z';

        case keys_1:  return '1';
        case keys_2:  return '2';
        case keys_3:  return '3';
        case keys_4:  return '4';
        case keys_5:  return '5';
        case keys_6:  return '6';
        case keys_7:  return '7';
        case keys_8:  return '8';
        case keys_9:  return '9';
        case keys_0:  return '0';

        case keys_enter: return '\n';
        case keys_tab: return '\t';
        case keys_acute:  return '`';
        case keys_hyphen:  return '-';
        case keys_equals:  return '=';
        case keys_space:  return ' ';
        case keys_apostrophe:  return '\'';
        case keys_comma: return ',';
        case keys_period: return '.';
        case keys_semicolon: return ';';
        case keys_lsquarebracket: return '[';
        case keys_rsquarebracket: return ']';
        case keys_forwardslash: return '/';
        case keys_backslash: return '\\';
        case keys_num_divide: return '/';
        case keys_num_multiply: return '*';
        case keys_num_subtract: return '-';
        case keys_num_add: return '+';
        case keys_num_comma: return '.';
        case keys_num_0: return '0';
        case keys_num_1: return '1';
        case keys_num_2: return '2';
        case keys_num_3: return '3';
        case keys_num_4: return '4';
        case keys_num_5: return '5';
        case keys_num_6: return '6';
        case keys_num_7: return '7';
        case keys_num_8: return '8';
        case keys_num_9: return '9';
        default:
            return -1;
    }
}

void kb_subscribe(struct kb_subscriber* subscriber)
{
   g_current_subscriber = subscriber;
}

void kb_unsubscribe(struct kb_subscriber* subscriber)
{
    g_current_subscriber = NULL;
}

static void reset_map()
{
    g_current_map = &g_current_set->maps[sc_map_index_1];
}

void kb_init()
{
    g_initial_ack_received = false;

    // Get scan-code info
    g_current_set = sc_get_set_1();
    reset_map();

    // Enable the keyboard
    pic_enable_irq(pic_irq_keyboard);
    OUTB(0x60, 0xF4); // Enable on the encoder
    OUTB(0x64, 0xAE); // Enable on the controller

    interrupt_receive(IRQ_1, kb_handle_interrupt);
}

static void kb_handle_interrupt(uint8_t irq, struct irq_regs* regs)
{
    uint8_t scan_code = INB(0x60);

    if(scan_code == 0xFA && !g_initial_ack_received) {

        // Currently, we seem to be getting an interrupt
        // right after enabling the keyboard and interrupts.
        // The scan code is 0xFA, and the current set is 1.
        // This is not a valid scan code, and we currently do not
        // know why this is being sent. We have only ever seen it
        // get "sent" once, in this instance. So we simply ignore it.
        g_initial_ack_received = true;
        pic_send_eoi(pic_irq_keyboard);
        return;
    }

    //terminal_write_string("KB IRQ Scan code: ");
    //terminal_write_hex(scan_code);
    //terminal_write_string("\n");

    // Translate scancode
    int sc_index = sc_get_entry_index(g_current_map, scan_code);

    if (sc_index < 0) {

        // Unknown scan-code
        terminal_write_string("Unknown scan code value: ");
        terminal_write_hex(scan_code);
        terminal_write_string("\n");
        // Reset to the first map in the set
        reset_map();
    }
    else {
        struct sc_map_entry* sc_entry = &g_current_map->entries[sc_index];

        switch (sc_entry->type) {
            case sc_map_entry_type_press:
                reset_map();
                if(g_current_subscriber->down != NULL) {
                    g_current_subscriber->down(sc_entry->data);
                }
                break;

            case sc_map_entry_type_release:
                reset_map();
                if(g_current_subscriber->up != NULL) {
                    g_current_subscriber->up(sc_entry->data);
                }
                break;

            case sc_map_entry_type_map:
                g_current_map = &g_current_set->maps[sc_entry->data];
                break;
        }
    }

    pic_send_eoi(pic_irq_keyboard);
}

