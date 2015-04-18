#include <types.h>
#include <stdbool.h>

#include <pic.h>
#include <pio.h>
#include <interrupt.h>
#include <kernel.h>
#include <keyboard.h>
#include <scan_code.h>

enum key_event_type {
    key_event_type_down,
    key_event_type_up
};

struct kb_subscriber*   g_current_subscriber;

struct sc_set*          g_current_set;
struct sc_map*          g_current_map;

static void kb_handle_interrupt(uint8_t irq, struct irq_regs* regs);

char kb_key_to_ascii(enum keys key)
{
    switch(key) {
        case keys_a:
            return 'a';
        case keys_b:
            return 'b';
        case keys_c:
            return 'c';
        case keys_up:
            return '^';
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

    // Translate scancode
    int sc_index = sc_get_entry_index(g_current_map, scan_code);

    if (sc_index < 0) {

        // Unknown scan-code

        // Reset to the first map in the set
        reset_map();
    }
    else {
        struct sc_map_entry* sc_entry = &g_current_map->entries[sc_index];

        switch (sc_entry->type) {
            case sc_map_entry_type_press:
                g_current_subscriber->down(sc_entry->data);
                reset_map();
                break;

            case sc_map_entry_type_release:
                g_current_subscriber->up(sc_entry->data);
                reset_map();
                break;

            case sc_map_entry_type_map:
                g_current_map = &g_current_set->maps[sc_entry->data];
                break;
        }
    }

    pic_send_eoi(pic_irq_keyboard);
}

