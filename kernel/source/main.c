#include "terminal.h"
#include "pci.h"
#include "pic.h"
#include "idt.h"
#include "pit.h"
#include "pio.h"

extern void isr_sysCallWrapper();
extern void isr_timerWrapper();

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

    // __asm("xchg %bx, %bx");
    idt_installHandler(0x80, (uint32_t)isr_sysCallWrapper, GateType_Trap32, 0); 
    idt_installHandler(IRQ_0, (uint32_t)isr_timerWrapper, GateType_Interrupt32, 0);

    // Enable keyboard interrupts
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);

    pic_initialize();

    //callTestSysCall(0x1234);

    terminal_writeString("Setting up PIT to fire at some point\n");
    idt_installHandler(0x80, (uint32_t)isr_sysCallWrapper, GateType_Trap32, 0); 

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
