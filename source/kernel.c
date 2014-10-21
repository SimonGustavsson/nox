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
#error "This kernel requires the i686-elf compiler"
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

	while(getNextUsbController(&devStartAddr, &dev))
	{
		terminal_writestring("Found (");
		terminal_writestring(hcNames[dev.progInterface]);
		terminal_writestring(") USB host controller on");

		terminal_writestring(" B:");
		print_int(devStartAddr.bus);
		terminal_writestring(" D:");
		print_int(devStartAddr.device);
		terminal_writestring(" F:");
		print_int(devStartAddr.func);
		terminal_writestring(" Addr:");

		// UHCI, it's special and uses baseAddr4
		print_int(dev.progInterface == 0 ? dev.baseAddr4 : dev.baseAddr0);
		
		terminal_writestring("\n");

		// Skip this dev when searching for next
		devStartAddr.func++;
	}

	terminal_writestring("Well I'm done... :-)\n");
}
