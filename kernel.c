#if !defined(__cplusplus)
#include <stdbool.h> // C Does not have bool :(
#endif
#include <stddef.h>
#include <stdint.h>
#include "terminal.h"
#include "pci.h"

#if defined(__linux__)
#error "You are not using a cross compiler!"
#endif

#if !defined(__i386__)
#error "This kernel requires the ix86-elf compiler"
#endif

const char* hcNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

#if defined(__cplusplus)
extern "C" /* Use C linkage for kernel_main. */
#endif
void kernel_main()
{
	terminal_initialize();

	/* Since there is no support for newlines in terminal_putchar yet, \n will
	   produce some VGA specific character instead. This is normal. */
	terminal_writestring("Hello, x86-World!\n");

	PCI_DEVICE dev;

	PCI_ADDRESS devStartAddr;
	devStartAddr.bus = 0;
	devStartAddr.device = 0;
	devStartAddr.func = 0;

	if(getNextUsbController(&devStartAddr, &dev))
	{
		terminal_writestring("Found a ");
		terminal_writestring(hcNames[dev.progInterface]);
		terminal_writestring(" USB host controller on \n");

		terminal_writestring("    Bus: ");
		print_int(devStartAddr.bus);
		terminal_writestring(" device: ");
		print_int(devStartAddr.device);
		terminal_writestring(" func: ");
		print_int(devStartAddr.func);
		terminal_writestring("\n");
		terminal_writestring("Base address: ");

		if(dev.progInterface == 0) // UHCI, it's special and uses baseAddr4
			print_int(dev.baseAddr4);
		else
			print_int(dev.baseAddr0);

		terminal_writestring("\n");
	}
	else
	{
		terminal_writestring("Couldn't find a usb device, boo!\n");
	}

	terminal_writestring("Well I'm done... :-)\n");
}
