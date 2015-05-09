#include <types.h>
#include <terminal.h>
#include "pci.h"
#include "uhci.h"
#include "pio.h"
// 15:11 - Reserved
//    10 - Interrupt disable
//     9 - Fast back-to-back enable
//     8 - SERR# Enable
//     7 - Reserved
//     6 - Parity Error Response
//     5 - VGA Palette Snoop
//     4 - Memory Write and Invalidate Enable
//     3 - Special cycles
//     2 - Bus Master
//     1 - Memory space
//     0 - I/O space

void uhci_init(uint32_t base_addr, pci_device* dev, struct pci_address* addr, uint8_t irq)
{
    bool memory_mapped = false;
    terminal_write_string("Calculating address...\n");
    if((base_addr | 0x1) == 0) {
        memory_mapped = true; 
        // Where in memory space this is is stored in bits 2:1
        uint8_t details = ((base_addr >> 1) & 0x3);
        switch(details) {
            case 0x0:
                // Anywhere in 32-bit space
                break;
            case 0x1:
                // Below 1MB
                
                break;
            case 0x2:
                {
                    // Anywhere in 64-bit space
                    unsigned long actual_addr = ((((uint64_t)dev->base_addr5) << 32) | base_addr);
                    break;
                }
        }
    }
    else {
        // It's an IO port mapped device
        // Scrap bit 1:0 as it's not a part of the address
        uint32_t io_addr = (base_addr & 0xFFFFFFFC);
        
        uint32_t original_value = IND(io_addr);
        OUTD(io_addr, 0xFFFFFFFF);
        uint32_t size = IND(io_addr);
        uint16_t range = (uint16_t) ( (~(size & ~0x1)) + 1);
        
        terminal_write_string("Address is: ");
        terminal_write_hex(io_addr);
        terminal_write_string(" - Range is: ");
        terminal_write_hex(range);
        terminal_write_string("\n");
        // Now write original value to PCI config space
        //  wut? Why the fuck why?
    }

    // Beore we initialize the card, make sure the cards I/O is disabled
    uint16_t cmd = pci_read_word(addr, PCI_COMMAND_REG_OFFSET);
    cmd = (cmd & ~0x1);
    pci_write_word(addr, PCI_COMMAND_REG_OFFSET, cmd);

    pci_write_dword(addr, 0x34, 0x00000000);
    pci_write_dword(addr, 0x38, 0x00000000);
    pci_write_byte(addr, 0x3C, irq);

    // Enable bus mastering and I/O access 
    pci_write_word(addr, 0x04, memory_mapped ? 0x06 : 0x05);

    int32_t detect_res = uhci_detect_root(base
}

// Detects the root device on the host-controller
int32_t uhci_detect_root(uint16_t baseAddr, bool ioAddr)
{
	if(!ioAddr)
		return -1; // Not currently supported

	for(int i = 0; i < 5; i++)
	{
	    OUTW(baseAddr + UHCI_CMD_OFFSET, 0x4);

		wait(11);

		OUTW(baseAddr + UHCI_CMD_OFFSET, 0x0);
	}

	if(INW(baseAddr + UHCI_CMD_OFFSET) != 0x0)
		return -2; // CMD Reg does not have its default value of 0

	if(INW(baseAddr + UHCI_STS_OFFSET) != 0x20)
		return -3; // Status Reg does not have its default value of 0x20

	// Clear out status reg (it's WC)
	OUTW(baseAddr + UHCI_STS_OFFSET, 0xFF);

	if(INW(baseAddr + UHCI_SOFMOD_OFFSET) != 0x40)
		return -4; // Start of Frame register does not have its default value of 0x40

	// Uf we set but 1, the controller should reset it to 0
	OUTW(baseAddr + UHCI_CMD_OFFSET, 0x2);

	wait(42); // arbitrary wait

	if(INW(baseAddr + UHCI_CMD_OFFSET) & 0x2)
		return -5; // Controller did not reset bit :(

	return 0; // Looks good
}
