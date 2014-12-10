#include "terminal.h"
#include "pci.h"
#include "pic.h"
#include "idt.h"

extern void isr_sysCallWrapper();

const char* gHcVersionNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

Idtd idtDescriptor = {};

IdtEntry idtEntries[256] = {};

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
    
    pic_initialize();

    __asm("int $0x80");

    while(1);
}

void isr_sysCall()
{
    terminal_writeString("0x80 Sys Call");
}
