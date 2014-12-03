#include "terminal.h"
#include "pci.h"

const char* gHcVersionNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

__attribute__((section(".text.boot"))) void _start()
{
    // This is how to break in Bochs
    //__asm("xchg %bx, %bx");

    terminal_initialize();
    terminal_writestring("NOX is here, bow down puny mortal...\n");

    PCI_DEVICE dev;
    PCI_ADDRESS addr;
    addr.bus = 0;
    addr.device = 0;
    addr.func = 0;    

    terminal_writestring("Finding USB host controllers...\n");

    while(getNextUsbController(&addr, &dev))
    {
        terminal_writestring("* Found ");
        terminal_writestring(gHcVersionNames[dev.progInterface]);
        terminal_writestring(" USB host controller.");
        addr.func++;
    }

	while(1);
}
