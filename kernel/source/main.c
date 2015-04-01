#include "terminal.h"
#include "pci.h"
#include "pic.h"
#include "idt.h"
#include "pit.h"
#include "pio.h"

#define SECTION_BOOT __attribute__((section(".text.boot"))) 

extern void isr_sysCallWrapper();
extern void isr_timerWrapper();
extern void isr_keyboardWrapper();
extern void isr_unknownWrapper();

const char* gHcVersionNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

idt_descriptor idtDescriptor = {};

idt_entry idtEntries[256] = {};

void call_test_sys_call(uint32_t foo)
{
    __asm("mov %0, %%eax"
            :
            : "r"(foo)
            : "eax");
    __asm("int $0x80");
}

SECTION_BOOT void _start()
{
    // This is how to break in Bochs
    //__asm("xchg %bx, %bx");

    terminal_init();
    terminal_write_string("NOX is here, bow down puny mortal...\n");

    idtDescriptor.limit = sizeof(idtEntries) - 1;
    idtDescriptor.base = (uint32_t)&idtEntries;
    idt_install(&idtDescriptor);

    for (int handlerIndex = 0x20; handlerIndex <= 0xFF; handlerIndex++) {
      idt_install_handler(handlerIndex, (uint32_t)isr_unknownWrapper, gate_type_interrupt32, 0);
    }

    idt_install_handler(0x80, (uint32_t)isr_sysCallWrapper, gate_type_trap32, 0);
    idt_install_handler(IRQ_0, (uint32_t)isr_timerWrapper, gate_type_interrupt32, 0);
    idt_install_handler(IRQ_1, (uint32_t)isr_keyboardWrapper, gate_type_interrupt32, 0);

    // Remap the interrupts fired by the PICs
    pic_init();

    // Re-enable interrupts, we're ready now!
    __asm("sti");

    // Enable the keyboard
    pic_enable_irq(PIC_IRQ_KEYBOARD);
    OUTB(0x60, 0xF4); // Enable on the encoder
    OUTB(0x64, 0xAE); // Enable on the controller

    //callTestSysCall(0x1234);

    pit_set(1000);

    terminal_write_string("\nKernel done, halting!\n");
    while(1);
}

void isr_sys_call(uint32_t foo)
{
    terminal_write_string("0x80 Sys Call, foo: ");
    terminal_write_hex(foo);
    terminal_write_string("\n");
}

void isr_keyboard()
{
    terminal_write_string("In keyboard ISR, scan code: ");
    uint8_t scanCode = INB(0x60);
    terminal_write_hex(scanCode);
    terminal_write_string("\n");
    pic_send_eoi(PIC_IRQ_KEYBOARD);
}

void isr_unknown()
{
    terminal_write_string("In unknown ISR\n");
}

