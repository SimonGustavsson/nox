#include <types.h>
#include <screen.h>
#include <terminal.h>
#include "pci.h"
#include "uhci.h"
#include "pio.h"
#include <pci.h>
#include <kernel.h>

// Forward declarations
static bool init_memory_mapped(pci_device* dev, uint32_t base_addr, uint8_t irq);
static bool init_memory_mapped32(uint32_t base_addr, uint8_t irq, bool below_1mb);
static bool init_memory_mapped64(uint64_t base_addr, uint8_t irq);

// TODO: Need a real wait - this currently doesn't work at all
//       But I've left it in here to ensure the rest of the code
//       compiles (though it currently does NOT work!)
void wait(uint16_t ms)
{
    volatile uint64_t moo = ms * 10;
    for(volatile uint64_t i = 0; i < moo; i++) {
    }
}

// Command register layout 
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
    terminal_write_uint32_x(base_addr);
    terminal_write_string("]\n");
}

static bool init_memory_mapped32(uint32_t base_addr, uint8_t irq, bool below_1mb)
{
    return false;
}

static bool init_memory_mapped64(uint64_t base_addr, uint8_t irq)
{
    return false;
}

static bool init_memory_mapped(pci_device* dev, uint32_t base_addr, uint8_t irq)
{
    // Where in memory space this is is stored in bits 2:1
    uint8_t details = ((base_addr >> 1) & 0x3);
    switch(details) {
        case 0x0: // Anywhere in 32-bit space 

            return init_memory_mapped32(base_addr, irq, false);
            break;
        case 0x1: // Below 1MB 
            return init_memory_mapped32(base_addr, irq, true);
            break;
        case 0x2: // Anywhere in 64-bit space 
            {
                unsigned long actual_addr = ((((uint64_t)dev->base_addr5) << 32) | base_addr);

                return init_memory_mapped64(actual_addr, irq);
                break;
            }
        default:
            {
                KERROR("Invalid address for memory mapped UHCI device");
                return false;
            }
    }
}

static bool init_port_io(uint32_t base_addr, uint8_t irq)
{
    // Scrap bit 1:0 as it's not a part of the address
    uint32_t io_addr = (base_addr & ~(1));

    terminal_write_string("I/O port: ");
    terminal_write_uint32_x(io_addr);
    terminal_write_string("\n");

    if(0 != uhci_detect_root(io_addr, true)) {
        KERROR("Failed to detect root device");
        return false;
    }

    terminal_write_string("UHCI root device successfully located.\n");

    return true;
}

void uhci_init(uint32_t base_addr, pci_device* dev, struct pci_address* addr, uint8_t irq)
{
    terminal_write_string("Initializing UHCI Host controller\n");
    terminal_indentation_increase();

    print_init_info(addr, base_addr);
    //
    bool result = false;
    if((base_addr | 0x1) == 0) {
        //uint32_t size = prepare_pci_device(addr, 9, true);

        result = init_memory_mapped(dev, base_addr, irq);
    }
    else {
        // It's an IO port mapped device
        result = init_port_io(base_addr, irq);
    }

    terminal_indentation_decrease();

    if(!result) {
        KERROR("Host controller initialization failed.");
    }
}

// Detects the root device on the host-controller
int32_t uhci_detect_root(uint16_t base_addr, bool ioAddr)
{
    // TODO: This currently assumes base_addr is a port I/O address!
    terminal_write_string("Searching for root device @ ");
    terminal_write_uint32_x(base_addr);
    terminal_write_string("\n");

    if(!ioAddr)
        return -1; // Not currently supported

    for(int i = 0; i < 5; i++)
    {
        OUTW(base_addr + UHCI_CMD_OFFSET, 0x4);

        wait(11111);

        // Reset the global reset bit
        // Note: We *must* wait 10ms before doing this
        // after setting the bit to 1
        OUTW(base_addr + UHCI_CMD_OFFSET, 0x0);
    }


    if(INW(base_addr + UHCI_CMD_OFFSET) != 0x0) {
        KERROR("CMD register does not have the default value '0'");
        return -2;
    }

    if(INW(base_addr + UHCI_STS_OFFSET) != 0x20) {
        KERROR("Status Reg does not have its default value of 0x20");
        return -3;
    }

    // Clear out status reg (it's WC)
    OUTW(base_addr + UHCI_STS_OFFSET, 0xFF);

    //if(INW(base_addr + UHCI_SOFMOD_OFFSET) != 0x40) {
    //    terminal_write_string("Unexpected SOFMOD value, expected 0x40\n");
    //	return -4;
    //}

    // If we set bit 1, the controller should reset it to 0
    OUTW(base_addr + UHCI_CMD_OFFSET, 0x2);

    wait(11111); // arbitrary wait

    if(INW(base_addr + UHCI_CMD_OFFSET) & 0x2) {
        KERROR("Controller did not reset bit :(");

        return -5;
    }

    return 0; // Looks good
}
