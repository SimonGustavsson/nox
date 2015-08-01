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

/* Note: All address in the UHCI driver assumes 32-bit addresses
 *       This is intentional and can *not* be changed, as UHCI only
 *       supports 32-bit addresses
*/

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
static bool init_io(uint32_t base_addr, uint8_t irq);
static bool init_mm(pci_device* dev, uint32_t base_addr, uint8_t irq);
static bool init_mm32(uint32_t base_addr, uint8_t irq, bool below_1mb);
static bool init_mm64(uint64_t base_addr, uint8_t irq);
#ifdef USB_DEBUG
static void print_init_info(struct pci_address* addr, uint32_t base_addr);
#endif
static int32_t detect_root(uint16_t base_addr, bool memory_mapped);
static void setup(uint32_t base_addr, uint8_t irq, bool memory_mapped);
static void uhci_irq(uint8_t irq, struct irq_regs* regs);
static bool enable_port(uint32_t base_addr, uint8_t port);
static uint32_t port_count_get(uint32_t base_addr);
static void schedule_init(uint32_t frame_list_addr);
// -------------------------------------------------------------------------
// Static Types
// -------------------------------------------------------------------------
enum mm_space {
    mm_space_32bit = 0,
    mm_space_below1mb = 1,
    mm_space_64bit = 2
};

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

enum frame_list_ptr {
    frame_list_ptr_terminate = 1 << 0,
    frame_list_ptr_queue     = 1 << 1
};

enum td_link_ptr {
    td_link_ptr_terminate   = 1 << 0,
    td_link_ptr_queue       = 1 << 1,
    td_link_ptr_depth_first = 1 << 2,
    // Bit 3 reserved
    // 31:4 Link pointer to another TD or Queue
};

enum td_status {
    td_status_bitstuff_error    = 1 << 17,
    td_status_crc_timeout       = 1 << 18,
    td_status_nak_received      = 1 << 19,
    td_status_babble_detected   = 1 << 20,
    td_status_data_buffer_error = 1 << 21,
    td_status_stalled           = 1 << 22,
    td_status_active            = 1 << 23
};

enum td_ctrl_status {
    // 10:0 Actual length
    // 15:11 reserved
    // 23:16 see td_status
    td_ctrl_status_ioc          = 1 << 24,
    td_ctrl_status_ios          = 1 << 25,
    td_ctrl_status_lowspeed     = 1 << 26,
    td_ctrl_status_3errors      = 3 << 27,
    td_ctrl_status_2errors      = 2 << 27,
    td_ctrl_status_1error       = 1 << 27,
    td_ctrl_status_short_packet = 1 << 29,
    // 31:30 reserved
};

enum td_token {
    // 7:0   Packet Identification
    // 14:8  Device Address
    // 18:15 End point
    td_ctrl_status_data_toggle = 1 << 19,
    // 20    reserved
    // 31:21 Maximum Length
};

struct transfer_descriptor {
    uint32_t link_ptr;
    uint32_t td_ctrl_status;
    uint32_t td_token;
    uint32_t buffer_ptr;    // 32-bit pointer to data of this transfer
    uint32_t software_use0;
    uint32_t software_use1;
    uint32_t software_use2;
    uint32_t software_use3;
} PACKED;

struct uhci_queue {
    uint32_t* head_link;
    uint32_t* element_link;

    // The host controller only cares about the first two
    // uint32 in this struct, the remaining fields are added because queues
    // have to be aligned to 16-byte boundary, making it easy to line up
    // multiple queues
    uint32_t padding1;
    uint32_t padding2;
} PACKED;

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

        result = init_mm(dev, base_addr, irq);
    }
    else {
        // It's an IO port mapped device
        result = init_io(base_addr, irq);
    }

    terminal_indentation_decrease();

    if(!result) {
        KERROR("Host controller initialization failed.");
    }
}

// -------------------------------------------------------------------------
// Initialization 
// -------------------------------------------------------------------------
static bool init_io(uint32_t base_addr, uint8_t irq)
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

    uint32_t port_count = port_count_get(io_addr);

    for(size_t i = 0; i < port_count; i++) {
        if(enable_port(io_addr, i)) {

            // Initialize our schedule skeleton
            uint32_t frame_list_addr = IND(io_addr + UHCI_FRAME_BASEADDR_OFFSET);
            schedule_init(frame_list_addr);

            // Happy days :-)
            terminal_write_string("Device on port ");
            terminal_write_uint32(i);
            terminal_write_string(" is ready for use\n");
        }
    }

    return true;
}

static bool init_mm(pci_device* dev, uint32_t base_addr, uint8_t irq)
{
    // Where in memory space this is is stored in bits 2:1
    uint8_t address_space = ((base_addr >> 1) & 0x3);
    switch(address_space) {
        case mm_space_32bit:    return init_mm32(base_addr, irq, false);
        case mm_space_below1mb: return init_mm32(base_addr, irq, true);
        case mm_space_64bit:    return init_mm64(((((uint64_t)dev->base_addr5) << 32) | base_addr), irq);
        default:
        {
            // TODO: Can we recover? assume 32-bit?
            KERROR("Invalid memory space for memory mapped UHCI device");
            return false;
        }
    }
}

static bool init_mm32(uint32_t base_addr, uint8_t irq, bool below_1mb)
{
    return false;
}

static bool init_mm64(uint64_t base_addr, uint8_t irq)
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

static uint32_t port_count_get(uint32_t base_addr)
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
    uint32_t offset = UHCI_PORTSC1_OFFSET + ((port - 1) * 2);
    uint16_t portsc = INW(base_addr + offset);
    uint16_t attempts = 0;

    //
    // First reset the port
    //

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

    //
    // Then enable the port
    //
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

enum nox_uhci_queue {
    nox_uhci_queue_1,        // Executed every 1ms
    nox_uhci_queue_2,        // Executed every 2ms
    nox_uhci_queue_4,        // Executed every 4ms
    nox_uhci_queue_8,        // ...etc...
    nox_uhci_queue_16,
    nox_uhci_queue_32,
    nox_uhci_queue_64,
    nox_uhci_queue_128,
    nox_uhci_queue_lowspeed,
    nox_uhci_queue_fullspeed,
    nox_uhci_queue_iso,       // Not sure what this means?
    nox_uhci_queue_reserved0, // Not used (yet)
    nox_uhci_queue_reserved1, // Not used (yet)
    nox_uhci_queue_reserved2, // Not used (yet)
    nox_uhci_queue_reserved3, // Not used (yet)
    nox_uhci_queue_reserved4, // Not used (yet)
};

struct uhci_queue g_root_queues[16] ALIGN(16) = {
    /*   1ms Queue */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*   2ms Queue */ {(uint32_t*)(uintptr_t)&g_root_queues[nox_uhci_queue_1], (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*   4ms Queue */ {(uint32_t*)(uintptr_t)&g_root_queues[nox_uhci_queue_2], (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*   8ms Queue */ {(uint32_t*)(uintptr_t)&g_root_queues[nox_uhci_queue_4], (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  16ms Queue */ {(uint32_t*)(uintptr_t)&g_root_queues[nox_uhci_queue_8], (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  32ms Queue */ {(uint32_t*)(uintptr_t)&g_root_queues[nox_uhci_queue_16], (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  64ms Queue */ {(uint32_t*)(uintptr_t)&g_root_queues[nox_uhci_queue_32], (uint32_t*)td_link_ptr_terminate, 0, 0},
    /* 128ms Queue */ {(uint32_t*)(uintptr_t)&g_root_queues[nox_uhci_queue_64], (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  Low speed  */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  Full speed */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*     ISO     */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  Reserved0  */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  Reserved1  */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  Reserved2  */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  Reserved3  */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
    /*  Reserved4  */ {(uint32_t*)td_link_ptr_terminate, (uint32_t*)td_link_ptr_terminate, 0, 0},
};

static void schedule_init(uint32_t frame_list_addr)
{
    // In Nox, we maintain 16 queues in the frame list, these queues
    // are static, and we should never have to modify the frame list after this point
    // If we want to queue a TD or Queue, we stick it in one of these pre-defined queues
    // Which will cause it to be executed at the desired interval
    //
    // nox_uhci_queue_1 executes every 1ms,
    // nox_uhci_queue_2 executes every 1ms etc.
    // This means the first frame will point to queue_1
    // The second frame will point to queue_2, which in turn points to queue_1 etc
    //
    // So our Frame List will end up looking like this (where --> means "points to"):
    // [0] --> queue_1
    // [1] --> queue_2 --> queue_1
    // [2] --> queue_1
    // [3] --> queue_4 --> queue_2 --> queue_1
    // NOTE: The queues have already been linked up at compile time, all we have to do now is install them!

    // Install them into the frame list
    uint32_t* frame_list = (uint32_t*)(uintptr_t)frame_list_addr;
    for(size_t i = 0; i < 1024; i++) {

        // Because the frames go from 0 -> 1023, we add one to the index
        // to figure out which type of queue to insert
        uint32_t frame_num = i + 1;

        if(frame_num % 128 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_128]) & frame_list_ptr_queue;
        }
        else if(frame_num % 64 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_64]) & frame_list_ptr_queue;
        }
        else if(frame_num % 32 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_32]) & frame_list_ptr_queue;
        }
        else if(frame_num % 16 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_16]) & frame_list_ptr_queue;
        }
        else if(frame_num % 8 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_8]) & frame_list_ptr_queue;
        }
        else if(frame_num % 4 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_4]) & frame_list_ptr_queue;
        }
        else if(frame_num % 2 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_2]) & frame_list_ptr_queue;
        }
        else if(frame_num % 1 == 0) {
           frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_1]) & frame_list_ptr_queue;
        }
    }
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
