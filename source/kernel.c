#include "terminal.h"

__attribute__((section(".text.boot"))) void _start()
{
    // This is how to break in Bochs
    //__asm("xchg %bx, %bx");

    terminal_initialize();
    terminal_writestring("NOX is here, bow down puny mortal...\n");
    terminal_writehex(0x1234);
	while(1);
}
