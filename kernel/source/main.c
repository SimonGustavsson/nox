#include "terminal.h"
#include "pci.h"
#include "pic.h"
#include "idt.h"
#include "pit.h"
#include "pio.h"

extern void isr_sysCallWrapper();
extern void isr_timerWrapper();
extern void isr_keyboardWrapper();
extern void isr_unknownWrapper();

const char* gHcVersionNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

Idtd idtDescriptor = {};

IdtEntry idtEntries[256] = {};

void callTestSysCall(uint32_t foo)
{
    __asm("mov %0, %%eax"
            :
            : "r"(foo)
            : "eax");
    __asm("int $0x80");
}

__attribute__((section(".text.boot"))) void _start()
{
    // This is how to break in Bochs
    //__asm("xchg %bx, %bx");

    terminal_initialize();
    terminal_writeString("NOX is here, bow down puny mortal...\n");

    idtDescriptor.limit = sizeof(idtEntries) - 1;
    idtDescriptor.base = (uint32_t)&idtEntries;
    idt_install(&idtDescriptor);

    for (int handlerIndex = 0x20; handlerIndex <= 0xFF; handlerIndex++) {
      idt_installHandler(handlerIndex, (uint32_t)isr_unknownWrapper, GateType_Interrupt32, 0);
    }

    idt_installHandler(0x80, (uint32_t)isr_sysCallWrapper, GateType_Trap32, 0);
    idt_installHandler(IRQ_0, (uint32_t)isr_timerWrapper, GateType_Interrupt32, 0);
    idt_installHandler(IRQ_1, (uint32_t)isr_keyboardWrapper, GateType_Interrupt32, 0);

    // Remap the interrupts fired by the PICs
    pic_initialize();

    // Re-enable interrupts, we're ready now!
    __asm("sti");

    // Enable the keyboard
    pic_enableIRQ(PIC_IRQ_KEYBOARD);
    outb(0x60, 0xF4); // Enable on the encoder
    outb(0x64, 0xAE); // Enable on the controller

    //callTestSysCall(0x1234);

    PIT_Set(1000);

    terminal_writeString("\nKernel done, halting!\n");
    while(1);
}

void isr_sysCall(uint32_t foo)
{
    terminal_writeString("0x80 Sys Call, foo: ");
    terminal_writeHex(foo);
    terminal_writeString("\n");
}

void isr_keyboard()
{
    terminal_writeString("In keyboard ISR, scan code: ");
    uint8_t scanCode = inb(0x60);
    terminal_writeHex(scanCode);
    terminal_writeString("\n");
    pic_sendEOI(PIC_IRQ_KEYBOARD);
}

void isr_unknown()
{
    terminal_writeString("In unknown ISR\n");
}

