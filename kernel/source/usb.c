#include <types.h>
#include <pci.h>
#include <screen.h>
#include <terminal.h>
#include <uhci.h>
#include <kernel.h>

// Foward declarations
static void print_debug_detect(struct pci_address* addr, pci_device* dev, const uint32_t base_addr, const char* type_str);
void usb_process_device(struct pci_address* addr, pci_device* dev);

void usb_init()
{
    terminal_write_string("Initializing USB host controllers\n");
    terminal_indentation_increase();

    // TODO: This should really check whether PCI is available before it tries to do anything with it
    struct pci_address addr;
    addr.bus = 0;
    addr.device = 0;
    addr.func = 0;
    pci_device dev;
    while(pci_device_get_next(&addr, USB_CLASS_CODE, USB_SUBCLASS_CODE, &dev)) {

        // NOTE: This function should NOT store the pointers passed in,
        //       as they're used for enumerating all devices
       usb_process_device(&addr, &dev);
       terminal_indentation_decrease();

       // This is lazy, stupid, and wrong.
       // But it doesn't break right now and allows me to
       // carry on with what I'm doing.
       // This should advance to the next POSSIBLE pci address
       // So that pci_get_next_usbhc starts scanning on the right one
       if(addr.func == 7) {
        addr.func = 0;
        addr.device++;
       }
       else {
        addr.func++;
       }
    } 

}

void usb_process_device(struct pci_address* addr, pci_device* dev)
{
    terminal_write_string("USB - Processing new device...\n");
    switch(dev->prog_interface) {
        case usb_hc_uhci:
            //print_debug_detect(addr, dev, dev->base_addr4, "UHCI");
            
            uhci_init(dev->base_addr4, dev, addr, 0x65); // TODO: Real IRQ number

            break;
        case usb_hc_ohci:
        {
            print_debug_detect(addr, dev, dev->base_addr0, "OHCI");

            KWARN("There is no OHCI driver!\n");
            break;
        }
            case usb_hc_ehci:
            print_debug_detect(addr, dev, dev->base_addr0, "EHCI");
            KWARN("There is no EHCI driver!\n");
            break;
        case usb_hc_xhci:
            print_debug_detect(addr, dev, dev->base_addr0, "xHCI");
            KWARN("There is no xHCI driver!\n");
            break;
    }
}

static void print_debug_detect(struct pci_address* addr, pci_device* dev, const uint32_t base_addr, const char* type_str)
{
    terminal_write_string("Found USB Host Controller (");
    terminal_write_string(type_str);
    terminal_write_string(") \n");
    terminal_write_string("  Bus: ");
    terminal_write_uint32(addr->bus);
    terminal_write_string(" Device: ");
    terminal_write_uint32(addr->device);
    terminal_write_string(" Function: ");
    terminal_write_uint32(addr->func);
    terminal_write_string(" Base: ");
    terminal_write_hex(base_addr);
    terminal_write_string("\n");
}

