#include <types.h>
#include <screen.h>
#include <terminal.h>
#include "pci.h"
#include "uhci.h"
#include "pio.h"
#include <pci.h>
#include <kernel.h>
#include <interrupt.h>
#include <pit.h>
#include <mem_mgr.h>

//#define USB_DEBUG

// -------------------------------------------------------------------------
// Documentation
// -------------------------------------------------------------------------
/* Command register layout 
   15:11 - Reserved
      10 - Interrupt disable
       9 - Fast back-to-back enable
       8 - SERR# Enable
       7 - Reserved
       6 - Parity Error Response
       5 - VGA Palette Snoop
       4 - Memory Write and Invalidate Enable
       3 - Special cycles
       2 - Bus Master
       1 - Memory space
       0 - I/O space
*/

// -------------------------------------------------------------------------
// Static Defines
// -------------------------------------------------------------------------
// TODO: Allocate mem for frame stack
#define UHCI_FRAME_STACK_ADDRESS (0x12345678)

// -------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------
static bool init_port_io(uint32_t base_addr, uint8_t irq);
static bool init_memory_mapped(pci_device* dev, uint32_t base_addr, uint8_t irq);
static bool init_memory_mapped32(uint32_t base_addr, uint8_t irq, bool below_1mb);
static bool init_memory_mapped64(uint64_t base_addr, uint8_t irq);
#ifdef USB_DEBUG
static void print_init_info(struct pci_address* addr, uint32_t base_addr);
#endif
static int32_t detect_root(uint16_t base_addr, bool memory_mapped);
static void setup(uint32_t base_addr, uint8_t irq, bool memory_mapped);
static void uhci_irq(uint8_t irq, struct irq_regs* regs);
static bool reset_hc_port(uint32_t base_addr, uint8_t port);
static bool enable_hc_port(uint32_t base_addr, uint8_t port);
static bool enable_port(uint32_t base_addr, uint8_t port);
static uint32_t get_port_count(uint32_t base_addr);
// -------------------------------------------------------------------------
// Static Types
// -------------------------------------------------------------------------
enum uhci_cmd {
    uhci_cmd_runstop        = 1 << 0,
    uhci_cmd_host_reset     = 1 << 1,
    uhci_cmd_global_reset   = 1 << 2,
    uhci_cmd_global_suspend = 1 << 3,
    uhci_cmd_force_g_resume = 1 << 4,
    uhci_cmd_swdebug        = 1 << 5,
    uhci_cmd_config_flag    = 1 << 6,
    uhci_cmd_max_packet     = 1 << 7,
    uhci_cmd_loopback_test  = 1 << 8
};

enum uhci_irpt {
    uhci_irpt_timeout_enable      = 1 << 0,
    uhci_irpt_resume_irpt_enable  = 1 << 1,
    uhci_irpt_oncomplete_enable   = 1 << 2,
    uhci_irpt_short_packet_enable = 1 << 3
};

enum uhci_portsc {
    // On startup: 0000_0101_1000_1010b
    // Not connected
    // Status has changed <--- really should be set, so that we will read bit 0 (on empty ports, this is 0)
    // NOT enabled
    // Enable has changed (will not be set on empty ports, as they can't be enabled)
    // Status D+ not set  \ these can't be set, as no device is attached
    // Status D- not set  /
    // Bit-7 set, always set, reserved
    // Low speed device attached
    // Port is not in reset   <--- This really shouldn't be set
    // Not suspended
    uhci_portsc_connect_status         = 1 << 0, // RO
    uhci_portsc_connect_status_changed = 1 << 1, // R/WC
    uhci_portsc_port_enabled           = 1 << 2, // R/W
    uhci_portsc_port_enabled_changed   = 1 << 3, // R/WC

    uhci_portsc_line_status_dpos       = 1 << 4, // RO
    uhci_portsc_line_status_dneg       = 1 << 5, // RO
    uhci_portsc_resume_detect          = 1 << 6, // R/W
    uhci_portsc_reserved               = 1 << 7, // Reserved
    uhci_portsc_low_speed_attached     = 1 << 8, // RO

    uhci_portsc_port_reset             = 1 << 9, // R/W
    uhci_portsc_suspend                = 1 << 12 // R/W
};

// -------------------------------------------------------------------------
// Public Contract
// -------------------------------------------------------------------------
void uhci_init(uint32_t base_addr, pci_device* dev, struct pci_address* addr, uint8_t irq)
{
    terminal_write_string("Initializing UHCI Host controller\n");
    terminal_indentation_increase();

#ifdef USB_DEBUG
    print_init_info(addr, base_addr);
#endif

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

// -------------------------------------------------------------------------
// Initialization 
// -------------------------------------------------------------------------
static bool init_port_io(uint32_t base_addr, uint8_t irq)
{
    // Scrap bit 1:0 as it's not a part of the address
    uint32_t io_addr = (base_addr & ~(1));

#ifdef USB_DEBUG
    terminal_write_string("I/O port: ");
    terminal_write_uint32_x(io_addr);
    terminal_write_string("\n");
#endif

    if(0 != detect_root(io_addr, false)) {
        KERROR("Failed to detect root device");
        return false;
    }

    setup(io_addr, irq, false);

    uint32_t port_count = get_port_count(io_addr);

    for(size_t i = 0; i < port_count; i++) {
        if(enable_port(io_addr, i)) {
            terminal_write_string("Device on port ");
            terminal_write_uint32(i);
            terminal_write_string(" is ready for use\n");
        }
    }

    return true;
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

static bool init_memory_mapped32(uint32_t base_addr, uint8_t irq, bool below_1mb)
{
    return false;
}

static bool init_memory_mapped64(uint64_t base_addr, uint8_t irq)
{
    return false;
}

static int32_t detect_root(uint16_t base_addr, bool memory_mapped)
{
#ifdef USB_DEBUG
    terminal_write_string("Searching for root device @ ");
    terminal_write_uint32_x(base_addr);
    terminal_write_string("\n");
#endif

    if(memory_mapped)
    {
        KWARN("Memory mapped UHCI is not supported!");
        return -1;
    }

    // To reset the UHCI we write send the global reset command
    // wait 10ms+, then reset the reset bit to 0 (HC won't do this for us)
    // five times
    for(int i = 0; i < 5; i++)
    {
        OUTW(base_addr + UHCI_CMD_OFFSET, uhci_cmd_global_reset);
        pit_wait(25);
        OUTW(base_addr + UHCI_CMD_OFFSET, 0x0);
    }

    if(INW(base_addr + UHCI_CMD_OFFSET) != 0x0) {
        KERROR("CMD register does not have the default value '0'");
        return -2;
    }

    if(INW(base_addr + UHCI_STATUS_OFFSET) != 0x20) {
        KERROR("Status Reg does not have its default value of 0x20");
        return -3;
    }

    // Clear out status reg (it's WC)
    OUTW(base_addr + UHCI_STATUS_OFFSET, 0x00FF);

    if(INB(base_addr + UHCI_SOFMOD_OFFSET) != 0x40) {
        KERROR("Sofmod register does not have its default value of 0x40");
        return -4;
    }

    // If we set bit 1, the controller should reset it to 0
    OUTW(base_addr + UHCI_CMD_OFFSET, uhci_cmd_host_reset);

    // Supposedly, we have to give the HC at least 42ms to reset
    pit_wait(42);

    if((INW(base_addr + UHCI_CMD_OFFSET) & uhci_cmd_host_reset) == uhci_cmd_host_reset) {
        KERROR("Controller did not reset bit :(");

        return -5;
    }

    return 0; // Looks good
}

static void setup(uint32_t base_addr, uint8_t irq, bool memory_mapped)
{
    if(memory_mapped)
    {
        KWARN("Cannot setup memory mapped UHCI");
        return;
    }

    // Enable interrupts
    OUTW(base_addr + UHCI_INTERRUPT_OFFSET,
            uhci_irpt_short_packet_enable |
            uhci_irpt_oncomplete_enable |
            uhci_irpt_resume_irpt_enable |
            uhci_irpt_timeout_enable);

    // Start on frame 0
    OUTW(base_addr + UHCI_FRAME_NUM_OFFSET, 0x0000);

    // Allocate a stack for use by the driver
    uint32_t stack_frame = (uint32_t)(uintptr_t)mem_page_get();
    OUTD(base_addr + UHCI_FRAME_BASEADDR_OFFSET, stack_frame);

    // The sofmod regisster *should* already be set to 0x40
    // as it's its default value after we reset, but just to be sure...
    OUTB(base_addr + UHCI_SOFMOD_OFFSET, 0x40);

    // Clear the entire status register (it's WC)
    OUTW(base_addr + UHCI_STATUS_OFFSET, 0xFFFF);

#ifdef USB_DEBUG
    terminal_write_string("UHCI set up for use\n");
#endif

    // Install the interrupt handler before we enable the schedule
    // So we don't miss any interrupts!
    interrupt_receive_trap(irq, uhci_irq);

    uint16_t cmd = INW(base_addr + UHCI_CMD_OFFSET);

    // We don't yet know if this HC supports 64-byte packets,
    // so just to be sure, set it to 32-byte packets
    cmd &= ~uhci_cmd_max_packet;

    // Go go go!
    cmd &= uhci_cmd_runstop;

    OUTW(base_addr + UHCI_CMD_OFFSET, cmd);
}

static uint32_t get_port_count(uint32_t base_addr)
{
    uint32_t offset = 0;
    uint32_t count = 0;
    uint32_t io_port = base_addr + UHCI_PORTSC1_OFFSET + offset;

    // Continue enumerating ports until we get a port that doesn't behave
    // like a port status/control register
    while(true) {

        //
        // Try to poke the reserved bit 7 to see if it behaves as expected
        //

        // Bit 7 is reserved, and always set to 1
        if((INW(io_port) & uhci_portsc_reserved) == 0)
            break;

        // Try to clear bit 7, we're not supposed to be able to
        // so if we manage to clear this bit, it means it's not a valid port
        OUTW(io_port, INW(io_port) & ~uhci_portsc_reserved);
        if((INW(io_port) & uhci_portsc_reserved) == 0)
            break;

        // Try to Write Clear bit 7, it's not supposed to do anything
        // If it does reset the bit, it's not a valid port register
        OUTW(io_port, INW(io_port) & uhci_portsc_reserved);
        if((INW(io_port) & uhci_portsc_reserved) == 0)
            break;

        //
        // Try to Write-Clear the Connected Status Changed and
        // the Port Enabled changed bits, do they clear?
        //
        OUTW(io_port, INW(io_port) | uhci_portsc_connect_status_changed | uhci_portsc_port_enabled_changed);
        if((INW(io_port) & (uhci_portsc_connect_status_changed | uhci_portsc_port_enabled_changed)) != 0)
            break;

        // if we got this far, we have a valid port
        // Increment the count and try the next register
        count++;
        io_port += 2;
    }

    return count;
}

static bool enable_port(uint32_t base_addr, uint8_t port)
{
    return reset_hc_port(base_addr, port) && enable_hc_port(base_addr, port);
}

static bool enable_hc_port(uint32_t base_addr, uint8_t port)
{
    uint32_t offset = UHCI_PORTSC1_OFFSET + ((port - 1) * 2);
    uint16_t portsc;
    uint16_t attempts = 0;

    do {
        portsc = INW(base_addr + offset);

        // If we have a connection changed or enabled changed, just reset the bits
        // and try again
        if( ((portsc & uhci_portsc_port_enabled_changed) == uhci_portsc_port_enabled_changed) ||
            ((portsc & uhci_portsc_connect_status_changed) == uhci_portsc_connect_status_changed)) {

            OUTW(base_addr + offset, portsc | uhci_portsc_port_enabled_changed | uhci_portsc_connect_status_changed);

            continue;
        }

        // Set it to enabled, and clear enabled changed whilst we're at it
        portsc |= (uhci_portsc_port_enabled ||
                   uhci_portsc_port_enabled_changed);

        OUTW(base_addr + offset, portsc);

        pit_wait(10);

        portsc = INW(base_addr + offset);
        attempts++;
    } while( ((portsc & uhci_portsc_port_enabled) != uhci_portsc_port_enabled) &&
            attempts < 10);

    return (portsc & uhci_portsc_port_enabled) != uhci_portsc_port_enabled;
}

static bool reset_hc_port(uint32_t base_addr, uint8_t port)
{
    uint32_t offset = UHCI_PORTSC1_OFFSET + ((port - 1) * 2);
    uint16_t portsc = INW(base_addr + offset);

    // Make sure the port is connected before we bother resetting it
    if((portsc & uhci_portsc_connect_status) != uhci_portsc_connect_status) {
        return false;
    }

    // Write enable bit
    portsc |= uhci_portsc_port_reset;
    OUTW(base_addr + offset, portsc);

    // USB specification says to give the HC 50ms to reset
    pit_wait(50);

    // Clear the bit, the port should be reset now
    portsc = INW(base_addr + offset);
    portsc &= ~uhci_portsc_port_reset;

    OUTW(base_addr + offset, portsc);

    // Wait 10ms for the recovery time
    pit_wait(10);

    return true;
}

// -------------------------------------------------------------------------
// IRQ Handler
// -------------------------------------------------------------------------
static void uhci_irq(uint8_t irq, struct irq_regs* regs)
{
    terminal_write_string("uhci irq!\n");
}

// -------------------------------------------------------------------------
// Static Utilities
// -------------------------------------------------------------------------
#ifdef USB_DEBUG
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
#endif
