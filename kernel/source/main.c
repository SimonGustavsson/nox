#include "terminal.h"
#include "pci.h"
#include "idt.h"

const char* gHcVersionNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

Idtd idtDescriptor = {};

IdtEntry idtEntries[256] = {};

__attribute__((section(".text.boot"))) void _start()
{
    // This is how to break in Bochs
    //__asm("xchg %bx, %bx");

    terminal_initialize();
    terminal_writeString("NOX is here, bow down puny mortal...\n");

    PciDevice dev;
    PciAddress addr;
    addr.bus = 0;
    addr.device = 0;
    addr.func = 0;    

    terminal_writeString("Finding USB host controllers...\n");

    while(getNextUsbController(&addr, &dev))
    {
        terminal_writeString("* Found ");
        terminal_writeString(gHcVersionNames[dev.progInterface]);
        terminal_writeString(" USB host controller.");
        addr.func++;
    }

    idtDescriptor.limit = sizeof(idtEntries) - 1;
    idtDescriptor.base = (uint32_t)&idtEntries;
    idt_install(&idtDescriptor);

    uint32_t idtAddressAsNumber = (uint32_t)idt_get();

    terminal_writeHex((uint32_t)&idtDescriptor);
    terminal_writeString(" == ");
    terminal_writeHex(idtAddressAsNumber);

	while(1);
}
