#include "pio.h"
#include "pic.h"
#include "terminal.h"

#define PIT_IO_PORT_CHANNEL_0 0x40
#define PIT_IO_PORT_CHANNEL_1 0x41
#define PIT_IO_PORT_CHANNEL_2 0x42
#define PIT_IO_PORT_COMMAND   0x43
#define PIT_BINARY_MODE 0 // 16-bit Binary mode
#define PIT_BCD_MODE 1    // four-digit BCD

// -------------------------------------------------------------------------
// Local types
// -------------------------------------------------------------------------

enum pit_mode {
    pit_mode_oneshot      = (0 << 1),
    pit_mode_retrigger    = (1 << 1),
    pit_mode_rate         = (2 << 1),
    pit_mode_square_wave  = (3 << 1),
    pit_mode_sw_strobe    = (4 << 1),
    pit_mode_hw_strobe    = (5 << 1),
    pit_mode_rate2        = (6 << 1),
    pit_mode_square_wave2 = (7 << 1)
};

enum pit_access {
    pit_access_latch = (0 << 4),
    pit_access_low   = (1 << 4),
    pit_access_high  = (2 << 4),
    pit_access_both  = (3 << 4) 
};

enum pit_channel {
    pit_channel_0        = (0 << 6),
    pit_channel_1        = (1 << 6),
    pit_channel_2        = (2 << 6),
    pit_channel_readback = (3 << 6),
};

// -------------------------------------------------------------------------
// Global variables
// -------------------------------------------------------------------------

int g_pit_counter = 0;

// -------------------------------------------------------------------------
// Exports
// -------------------------------------------------------------------------

void pit_set(uint16_t frequency_divisor)
{
    g_pit_counter = 0;

    // Not gonna get any timer interrupts if we don't
    // turn them on dawg
    pic_enable_irq(pic1_irq_timer);

    // Set the PIT to fire off an interrupt in the specified time
    OUTB(PIT_IO_PORT_COMMAND, pit_mode_oneshot|
                          pit_channel_0 |
                        pit_access_both);

    uint16_t reload_value = frequency_divisor; // this correct?

    OUTB(PIT_IO_PORT_CHANNEL_0, (uint8_t)(reload_value & 0xFF));
    OUTB(PIT_IO_PORT_CHANNEL_0, (uint8_t)((reload_value >> 8) & 0xFF));

    terminal_write_string("I set up us the PIT\n");
}

void isr_timer()
{
    // The amount of time we set to wait has now passed
    g_pit_counter++;

    terminal_write_string("Holy shitballs, Timer interrupt!!\n");

    if(g_pit_counter == 140) {
        terminal_write_string("Timer hit!\n");
        g_pit_counter = 0;
    }

    // To keep track of time in the kernel - 
    // we set the interrupt to fire again
    //PIT_Set(1000);

    // Tell the PIC we have handled the interrupt
    pic_send_eoi(pic1_irq_timer);
}

