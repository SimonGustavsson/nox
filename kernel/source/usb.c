#include <types.h>
#include <pci.h>
#include <screen.h>
#include <terminal.h>
#include <uhci.h>
#include <kernel.h>

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

void usb_process_device(struct pci_address* addr, pci_device* dev)
{
    switch(dev->prog_interface) {
        case usb_hc_uhci:
            uhci_init(dev->base_addr4, dev, addr, 0x65); // TODO: Real IRQ number

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

