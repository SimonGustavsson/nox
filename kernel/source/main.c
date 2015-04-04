#include <kernel.h>
#include <debug.h>
#include <types.h>
#include <terminal.h>
#include <pci.h>
#include <pic.h>
#include <interrupt.h>
#include <pit.h>
#include <pio.h>

const char* gHcVersionNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

void call_test_sys_call(uint32_t foo)
{
    __asm("mov %0, %%eax"
            :
            : "r"(foo)
            : "eax");
    __asm("int $0x80");
}

void isr_syscall(uint8_t irq, struct irq_regs* regs)
{
    terminal_write_string("0x80 Sys Call, foo: ");
    terminal_write_hex(regs->eax);
    terminal_write_string("\n");
}

void isr_keyboard(uint8_t irq, struct irq_regs* regs)
{
    terminal_write_string("In keyboard ISR, scan code: ");
    uint8_t scanCode = INB(0x60);
    terminal_write_hex(scanCode);
    terminal_write_string("\n");
    pic_send_eoi(PIC_IRQ_KEYBOARD);
}

SECTION_BOOT void _start()
{
    terminal_init();
    terminal_write_string("NOX is here, bow down puny mortal...\n");

    interrupt_init_system();
    interrupt_receive_trap(0x80, isr_syscall);
    interrupt_receive(IRQ_1, isr_keyboard);

    // Remap the interrupts fired by the PICs
    pic_init();

    // Re-enable interrupts, we're ready now!
    interrupt_enable_all();

    // Enable the keyboard
    pic_enable_irq(PIC_IRQ_KEYBOARD);
    OUTB(0x60, 0xF4); // Enable on the encoder
    OUTB(0x64, 0xAE); // Enable on the controller

    call_test_sys_call(0x1234);

    pit_set(1000);

    terminal_write_string("\nKernel done, halting!\n");
    while(1);
}
