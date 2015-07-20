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
static void start_pit();

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
static volatile uint64_t g_pit_ticks = 0;
static uint32_t g_default_pit_divisor = (1193180 / 1000); // Once per millisecond

// -------------------------------------------------------------------------
// Exports
// -------------------------------------------------------------------------
void pit_init()
{

    // Not gonna get any timer interrupts if we don't
    // turn them on dawg
    interrupt_receive(IRQ_0, isr_timer);
    pic_enable_irq(pic_irq_timer);

    // Set the PIT to fire off an interrupt in the specified time
    OUTB(PIT_IO_PORT_COMMAND, pit_mode_oneshot|
            pit_channel_0 |
            pit_access_both);

    start_pit();
}

void pit_wait(size_t ms)
{
    // The PIT ticks once every millisecond
    uint32_t now = g_pit_ticks;
    uint32_t end = now + ms;

    while(g_pit_ticks < end) {
        __asm("HLT");
    }
}

// -------------------------------------------------------------------------
// Static Functions
// -------------------------------------------------------------------------
static void start_pit()
{
    OUTB(PIT_IO_PORT_CHANNEL_0, (uint8_t)(g_default_pit_divisor & 0xFF));
    OUTB(PIT_IO_PORT_CHANNEL_0, (uint8_t)((g_default_pit_divisor >> 8) & 0xFF));
}

static void isr_timer(uint8_t irq, struct irq_regs* regs)
{
    // The amount of time we set to wait has now passed
    g_pit_ticks++;

    // To keep track of time in the kernel -
    // we set the interrupt to fire again
    start_pit();

    // Tell the PIC we have handled the interrupt
    pic_send_eoi(pic_irq_timer);
}

