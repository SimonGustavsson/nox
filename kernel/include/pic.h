#include "stdint.h"
#include "pio.h"

/*  Addr   IRQ      Desc
	0x000	0		Divide by 0
	0x004	1		Single step (Debugger)
	0x008	2		Non Maskable Interrupt (NMI) Pin
	0x00C	3		Breakpoint (Debugger)
	0x010	4		Overflow
	0x014	5		Bounds check
	0x018	6		Undefined Operation Code (OPCode) instruction
	0x01C	7		No coprocessor
	0x020	8		Double Fault
	0x024	9		Coprocessor Segment Overrun
	0x028	10		Invalid Task State Segment (TSS)
	0x02C	11		Segment Not Present
	0x030	12		Stack Segment Overrun
	0x034	13		General Protection Fault (GPF)
	0x038	14		Page Fault
	0x03C	15		Unassigned
	0x040	16		Coprocessor error
	0x044	17		Alignment Check (486+ Only)
	0x048	18		Machine Check (Pentium/586+ Only)
	0x05C	19-31	Reserved exceptions
	0x068 - 0x3FF	32-255	Interrupts free for software use
*/

// IRQ remapping for protected mode
#define IRQ_0 0x20                // IRQ 0-7 mapped to 0x20-0x27 - this is the PIT
#define IRQ_1 (IRQ_0 + 1)         // This is the Keyboard
#define IRQ_8 0x28                // IRQ 8-15 mapped to 0x28-0x36

// IRQs handled by the master pic
enum pic_irq {
    pic_irq_timer       = 0,
    pic_irq_keyboard    = 1,
    pic_irq_slave       = 2,
    pic_irq_serial2     = 3,
    pic_irq_serial1     = 4,
    pic_irq_parallel2   = 5,
    pic_irq_diskette    = 6,
    pic_irq_parallel1   = 7,

    // IRQs handled by the slave pic
    // Note: The actual IRQ number for these are 0-6,
    //       we subtract 8 from all IRQs > 7 and send to
    //       the slave, that's why these all have a value
    //       that is 8 higher than the real IRQ number
    pic_irq_cmos_timer  = 8,
    pic_irq_caret_trace = 9,
    pic_irq_aux         = 10,
    pic_irq_fpu         = 11,
    pic_irq_hdc         = 12
};

void pic_init();
void pic_send_eoi(uint8_t irq);
void pic_enable_irq(uint8_t irq);
void pic_disable_irq(uint8_t irq);

