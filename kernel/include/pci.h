#ifndef PCI_H
#define PCI_H

#define  PCI_ADDR     0x0CF8
#define  PCI_DATA     0x0CFC

#define MAX_PCI_BUS_NR 256
#define MAX_PCI_BUS_DEV_NR 32
#define MAX_FUNC_PER_PCI_BUS_DEV 8

#define USB_CLASS_CODE (0xC)
#define USB_SUBCLASS_CODE (0x03)

typedef struct {
     uint16_t vendor_id;           // Vendor ID = FFFFh if not 'a valid entry'
     uint16_t device_id;           // Device ID
     uint16_t command;             // Command
     uint16_t status;              // Status
     uint8_t  revision_id;         // Revision ID
     uint8_t  prog_interface;      //
     uint8_t  dev_sub_class;       //
     uint8_t  devClass;            //
     uint8_t  cache_line_size;     // Cache Line Size
     uint8_t  latency_timer;       // Latency Timer
     uint8_t  header_type;         // Header Type (0,1,2, if bit 7 set, multifunction device)
     uint8_t  self_test;
     uint32_t base_addr0;          // This one for OHCI, EHCI, and xHCI
     uint32_t base_addr1;          //
     uint32_t base_addr2;          //
     uint32_t base_addr3;          //
     uint32_t base_addr4;          // This one for UHCI
     uint32_t base_addr5;          //
     uint32_t card_bus;            //
     uint16_t sub_sys_vendor_id;   //
     uint16_t sub_sys_id;          //
     uint32_t rom_base_addr;       //
     uint8_t  caps_off;            //
     uint8_t  resv0[3];            //
     uint32_t resv1;               //
     uint8_t  irq;                 //
     uint8_t  int_pin;             //
     uint8_t  min_time;            //
     uint8_t  max_time;            //
     uint32_t dev_data[48];        // varies by device.
} pci_device;

typedef struct {
    uint32_t bus;
    uint32_t device;
    uint32_t func;
} pci_address;

uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len);
void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len, uint32_t value);
bool pci_get_next_usbhc(pci_address* addr, pci_device* result);

#endif
