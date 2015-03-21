#include "pio.h"
#include "pic.h"
#include "terminal.h"

#define PIT_CHANNEL_0_PORT 0x40
#define PIT_CHANNEL_1_PORT 0x41
#define PIT_CHANNEL_2_PORT 0x42
#define PIT_COMMAND_PORT   0x43

#define PIT_MODE_ONESHOT      (0 << 1)
#define PIT_MODE_RETRIGGER    (1 << 1)
#define PIT_MODE_RATE         (2 << 1)
#define PIT_MODE_SUARE_WAVE   (3 << 1)
#define PIT_MODE_SW_STROBE    (4 << 1)
#define PIT_MODE_HW_STROBE    (5 << 1)
#define PIT_MODE_RATE2        (6 << 1)
#define PIT_MODE_SQUARE_WAVE2 (7 << 1)

#define PIT_ACCESS_LATCH (0 << 4)
#define PIT_ACCESS_LOW   (1 << 4)
#define PIT_ACCESS_HIGH  (2 << 4)
#define PIT_ACCESS_BOTH  (3 << 4)

#define PIT_CHANNEL_0        (0 << 6)
#define PIT_CHANNEL_1        (1 << 6)
#define PIT_CHANNEL_2        (2 << 6)
#define PIT_CHANNEL_READBACK (3 << 6)

#define PIT_BINARY_MODE 0 // 16-bit Binary mode
#define PIT_BCD_MODE 1    // four-digit BCD

int gPitCounter = 0;

void PIT_Set(uint16_t frequencyDivisor) {
    gPitCounter = 0;

    // Not gonna get any timer interrupts if we don't
    // turn them on dawg
    pic_enableIRQ(PIC_IRQ_TIMER);

    // Set the PIT to fire off an interrupt in the specified time
    outb(PIT_COMMAND_PORT, PIT_MODE_ONESHOT |
                          PIT_CHANNEL_0 |
                        PIT_ACCESS_BOTH);

    uint16_t reloadValue = frequencyDivisor; // this correct?

    outb(PIT_CHANNEL_0_PORT, (uint8_t)(reloadValue & 0xFF));
    outb(PIT_CHANNEL_0_PORT, (uint8_t)((reloadValue >> 8) & 0xFF));

    terminal_writeString("I set up us the PIT, we have signal\n");
}

void isr_timer() {

    // The amount of time we set to wait has now passed
    gPitCounter++;

    terminal_writeString("Holy shitballs, Timer interrupt!!\n");

    if(gPitCounter == 140) {
        terminal_writeString("Timer hit!\n");
        gPitCounter = 0;
    }

    //PIT_Set(1000);

   // Tell the PIC we have handled the interrupt
    pic_sendEOI(PIC1_CTRL);
}

