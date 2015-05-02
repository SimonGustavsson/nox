#include <kernel.h>
#include "pio.h"
#include "pic.h"
#include "terminal.h"
#include <interrupt.h>

#define PIT_IO_PORT_CHANNEL_0 0x40
#define PIT_IO_PORT_CHANNEL_1 0x41
#define PIT_IO_PORT_CHANNEL_2 0x42
#define PIT_IO_PORT_COMMAND   0x43
#define PIT_BINARY_MODE 0 // 16-bit Binary mode
#define PIT_BCD_MODE 1    // four-digit BCD

// -------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------
static void isr_timer(uint8_t irq, struct irq_regs* irq_regs);

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
static int g_pit_counter = 0;

// -------------------------------------------------------------------------
// Exports
// -------------------------------------------------------------------------
void pit_set(uint16_t frequency_divisor)
{

    // Not gonna get any timer interrupts if we don't
    // turn them on dawg
    interrupt_receive(IRQ_0, isr_timer);
    pic_enable_irq(pic_irq_timer);

    // Set the PIT to fire off an interrupt in the specified time
    OUTB(PIT_IO_PORT_COMMAND, pit_mode_oneshot|
            pit_channel_0 |
            pit_access_both);

    uint16_t reload_value = frequency_divisor; // this correct?

    OUTB(PIT_IO_PORT_CHANNEL_0, (uint8_t)(reload_value & 0xFF));
    OUTB(PIT_IO_PORT_CHANNEL_0, (uint8_t)((reload_value >> 8) & 0xFF));
}

uint32_t g_num_timer_hits = 0;

static void isr_timer(uint8_t irq, struct irq_regs* regs)
{
    // The amount of time we set to wait has now passed
    g_pit_counter++;

    // Show a message roughly once every couple of seconds
    if(g_pit_counter == 36) {
        g_num_timer_hits++;
        terminal_write_string("Timer hit!");
        terminal_write_hex(g_num_timer_hits);
        terminal_write_string("\n");
        g_pit_counter = 0;
    }

    // To keep track of time in the kernel -
    // we set the interrupt to fire again
    //
    // 0 gives a divisor of 65,536, which yields an interval
    // of ~18Hz
    //pit_set(0);

    // Tell the PIC we have handled the interrupt
    pic_send_eoi(pic_irq_timer);
}
