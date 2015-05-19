#include <types.h>
#include <screen.h>
#include <terminal.h>
#include "pci.h"
#include "uhci.h"
#include "pio.h"
#include <pci.h>
#include <kernel.h>

// Forward declarations
static bool init_memory_mapped(pci_device* dev, uint32_t base_addr, uint32_t size, uint8_t irq);
static bool init_memory_mapped32(uint32_t base_addr, uint32_t size, uint8_t irq, bool below_1mb);
static bool init_memory_mapped64(uint64_t base_addr, uint32_t size, uint8_t irq);

// TODO: Need a real wait - this currently doesn't work at all
//       But I've left it in here to ensure the rest of the code
//       compiles (though it currently does NOT work!)
void wait(uint16_t ms)
{
   volatile uint64_t moo = ms * 10;
   for(volatile uint64_t i = 0; i < moo; i++) {
   }
}

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
static void print_init_info(struct pci_address* addr, uint32_t base_addr)
{
    terminal_write_string("PCI Address [Bus: ");
    terminal_write_uint32(addr->bus);
    terminal_write_string(" Device: ");
    terminal_write_uint32(addr->device);
    terminal_write_string(" Function: ");
    terminal_write_uint32(addr->func);
    terminal_write_string(" Base: ");
    terminal_write_hex(base_addr);
    terminal_write_string("]\n");
}

static void prepare_pci_device(struct pci_address* addr, uint8_t irq, bool memory_mapped)
{
    // Beore we initialize the card, make sure the cards I/O is disabled
    uint16_t cmd = pci_read_word(addr, PCI_COMMAND_REG_OFFSET);
    cmd = (cmd & ~0x1);
    pci_write_word(addr, PCI_COMMAND_REG_OFFSET, cmd);

    // Now try to get the size of the address space

    pci_write_dword(addr, 0x34, 0x00000000);
    pci_write_dword(addr, 0x38, 0x00000000);
    pci_write_byte(addr, 0x3C, irq);

    // Enable bus mastering and I/O access 
    pci_write_word(addr, 0x04, memory_mapped ? 0x06 : 0x05);
}

static bool init_memory_mapped32(uint32_t base_addr, uint32_t size, uint8_t irq, bool below_1mb)
{
    return false;
}

static bool init_memory_mapped64(uint64_t base_addr, uint32_t size, uint8_t irq)
{
    return false;
}

static bool init_memory_mapped(pci_device* dev, uint32_t base_addr, uint32_t size, uint8_t irq)
{
    // Where in memory space this is is stored in bits 2:1
    uint8_t details = ((base_addr >> 1) & 0x3);
    switch(details) {
        case 0x0: // Anywhere in 32-bit space 

            return init_memory_mapped32(base_addr, size, irq, false);
            break;
        case 0x1: // Below 1MB 
            return init_memory_mapped32(base_addr, size, irq, true);
            break;
        case 0x2: // Anywhere in 64-bit space 
        { 
            unsigned long actual_addr = ((((uint64_t)dev->base_addr5) << 32) | base_addr);

            return init_memory_mapped64(actual_addr, size, irq);
            break;
        }
        default:
        {
            KERROR("Invalid address for memory mapped UHCI device");
            return false;
        }
    }
}

static bool init_port_io(uint32_t base_addr, uint32_t size, uint8_t irq)
{
    // Scrap bit 1:0 as it's not a part of the address
    uint32_t io_addr = (base_addr & ~(1));

    terminal_write_string("I/O port: ");
    terminal_write_hex(io_addr);
    terminal_write_string(", size: ");
    terminal_write_hex(size);
    terminal_write_string("\n");

    if(0 != uhci_detect_root(io_addr, true)) {
        KERROR("Failed to detect root device");
        return false;
    }

    return true;
}

void uhci_init(uint32_t base_addr, pci_device* dev, struct pci_address* addr, uint8_t irq)
{
    terminal_write_string("Initializing UHCI Host controller\n");
    terminal_indentation_increase();

    print_init_info(addr, base_addr);

    uint32_t size = pci_device_get_memory_size(addr, PCI_BASE_ADDR4_REG_OFFSET);
    terminal_write_string("SIze is: ");
    terminal_write_hex(size);
    terminal_write_string("\n");

    // Get the size of the memory region this device occupies
    bool result = false;
    if((base_addr | 0x1) == 0) {
        result = init_memory_mapped(dev, base_addr, size, irq);
    }
    else {
        // It's an IO port mapped device
        result = init_port_io(base_addr, size, irq);
    }

    terminal_indentation_decrease();

    if(!result) {
        KERROR("Host controller initialization failed.");
    }
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

	if(INW(baseAddr + UHCI_CMD_OFFSET) != 0x0) {
        KERROR("CMD register does not have the default value '0'");
		return -2;
    }

	if(INW(baseAddr + UHCI_STS_OFFSET) != 0x20) {
        KERROR("Status Reg does not have its default value of 0x20");
		return -3;
    }

	// Clear out status reg (it's WC)
	OUTW(baseAddr + UHCI_STS_OFFSET, 0xFF);

	if(INW(baseAddr + UHCI_SOFMOD_OFFSET) != 0x40) {
        KERROR("Start of Frame register does not have its default value of 0x40");
		return -4;
    }

	// If we set bit 1, the controller should reset it to 0
	OUTW(baseAddr + UHCI_CMD_OFFSET, 0x2);

	wait(42); // arbitrary wait

	if(INW(baseAddr + UHCI_CMD_OFFSET) & 0x2) {
        KERROR("Controller did not reset bit :(");
 
		return -5;
    }

	return 0; // Looks good
}
