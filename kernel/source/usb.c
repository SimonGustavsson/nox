#include <types.h>
#include <pci.h>
#include <screen.h>
#include <terminal.h>
#include <uhci.h>
#include <kernel.h>

#define UHCI_REV_1_0 0x10
#define UHCI_IRQ 9 // TODO: What IRQ should we use? What about multiple HCs? Share IRQ? eww?

// Foward declarations
static void usb_process_device(struct pci_address* addr, pci_device* dev);
static bool advance_to_next_address(struct pci_address* address);

void usb_init()
{
    terminal_write_string("Initializing USB host controllers\n");
    terminal_indentation_increase();

    // TODO: This should really check whether PCI is available before it tries to do anything with it
    struct pci_address addr = {};
    pci_device dev;
    while(pci_device_get_next(&addr, USB_CLASS_CODE, USB_SUBCLASS_CODE, &dev)) {

        // NOTE: This function should NOT store the pointers passed in,
        //       as they're used for enumerating all devices
       usb_process_device(&addr, &dev);

       if(!advance_to_next_address(&addr)) {
           break;
       }
    } 

    terminal_indentation_decrease();
}

static bool advance_to_next_address(struct pci_address* address)
{
   if(++address->func >= MAX_FUNC_PER_PCI_BUS_DEV) {
        address->func = 0;
        
        if(++address->device >= MAX_PCI_BUS_DEV_NR) {
            address->device = 0;

            if(++address->bus >= MAX_PCI_BUS_NR) {
                return false; // Reached the end
            }
        }
   } 

   return true;
}

static void process_uhci(struct pci_address* addr, pci_device* dev)
{
    uint8_t revision = pci_read_byte(addr, 0x60);
    if(revision != UHCI_REV_1_0) {
        // This host controller indicates that it supports a version
        // of the USB specification that is *not* 1.0. Ignore it
        KWARN("Detected UHCI with unsupported revision, ignoring...");
        return;
    }

    // Bit 0 indicates whether it's memory mapped or port I/O
    bool memory_mapped = (dev->base_addr4 & 0x1) == 1;

    // Beore we initialize the card, make sure the cards I/O is disabled
    uint16_t cmd = pci_read_word(addr, PCI_COMMAND_REG_OFFSET);
    cmd = (cmd & ~0x1);
    pci_write_word(addr, PCI_COMMAND_REG_OFFSET, cmd);

    // THe USB book I'm reading is telling me to null out the
    // capabilities register as well as the two registers marked as "reserved"
    // Not sure why, TODO: Investigate! :-S
    pci_write_dword(addr, PCI_CAPS_OFF_REG_OFFSET, 0x00000000);
    pci_write_dword(addr, 0x38, 0x00000000);

    // Set the IRQ
    pci_write_byte(addr, PCI_IRQ_REG_OFFSET, UHCI_IRQ);

    // Now try to get the size of the address space
    //uint32_t size = pci_device_get_memory_size(addr, PCI_BASE_ADDR4_REG_OFFSET);
    // Enable bus mastering and I/O access 
    pci_write_word(addr, 0x04, memory_mapped ? 0x06 : 0x05);

    // Disable legacy support and clear current status
    pci_write_word(addr, PCI_LEG_SUP_REG_OFFSET,
            PCI_LEGACY_PTS | // Clear Sequence ended bit
            PCI_LEGACY_TBY64W | PCI_LEGACY_TBY64R | PCI_LEGACY_TBY60W | PCI_LEGACY_TBY60R); // Clear status

    // The device is now ready for the UHCI driver to take over
    uhci_init(dev->base_addr4, dev, addr, 0x65); // TODO: Real IRQ number
}

void usb_process_device(struct pci_address* addr, pci_device* dev)
{
    switch(dev->prog_interface) {
        case usb_hc_uhci:
            process_uhci(addr, dev);
            break;
        case usb_hc_ohci:
            KWARN("Found unsupported OHCI controller, ignoring...");
            break;
        case usb_hc_ehci:
            KWARN("Found unsupported EHCI controller, ignoring...");
            break;
        case usb_hc_xhci:
            KWARN("Found unsupported xHCI controller, ignoring...");
            break;
    }
}

