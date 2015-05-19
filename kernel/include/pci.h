#ifndef NOX_PCI_H
#define NOX_PCI_H

// Only read and write 32-bit values to these ports!
// The 32-bit value WRITTEN to the ADDR io port should have the following format:
//
// 31: Enable configuration space mapping
// 30:24 | reserved
// 23:16 | Bus number
// 15:11 | Device number
// 10:8  | Function number
//  7:2  | Configuration register number
//  1:0  | reserved 
#define  PCI_ADDR_IO_PORT     0x0CF8
#define  PCI_DATA_IO_PORT     0x0CFC

#define PCI_ENABLE_CONFIG_SPACE_MAPPING (0x80000000)

#define MAX_PCI_BUS_NR 256
#define MAX_PCI_BUS_DEV_NR 32
#define MAX_FUNC_PER_PCI_BUS_DEV 8

#define USB_CLASS_CODE (0xC)
#define USB_SUBCLASS_CODE (0x03)

// Offset of standard PCI Configuration space registers
#define PCI_VENDOR_ID_REG_OFFSET           0x00
#define PCI_DEVICE_ID_REG_OFFSET           0x02
#define PCI_COMMAND_REG_OFFSET             0x04
#define PCI_STATUS_REG_OFFSET              0x06
#define PCI_REVISION_ID_OFFSET             0x08
#define PCI_PROG_INTERFACE_REG_OFFSET      0x09
#define PCI_DEV_SUB_CLASS_REG_OFFSET       0x0A
#define PCI_DEV_CLASS_REG_OFFSET           0x0B
#define PCI_CACHE_LINE_SIZE_REG_OFFSET 0x0C
#define PCI_LATENCY_TIMER_REG_OFFSET   0x0D
#define PCI_HEADER_TYPE_REG_OFFSET     0x0E
#define PCI_SELF_TEST_REG_OFFSET       0x0F
#define PCI_BASE_ADDR0_REG_OFFSET      0x10
#define PCI_BASE_ADDR1_REG_OFFSET      0x14
#define PCI_BASE_ADDR2_REG_OFFSET      0x18
#define PCI_BASE_ADDR3_REG_OFFSET      0x1C
#define PCI_BASE_ADDR4_REG_OFFSET      0x20
#define PCI_BASE_ADDR5_REG_OFFSET      0x24
#define PCI_CARD_BUS_REG_OFFSET        0x28
#define PCI_SUB_SYS_VENDOR_ID_REG_OFFSET 0x2C
#define PCI_SUB_SYSTEM_ID_REG_OFFSET  0x2E
#define PCI_ROM_BASE_ADDR_REG_OFFSET 0x30
#define PCI_CAPS_OFF_REG_OFFSET 0x34
#define PCI_IRQ_REG_OFFSET 0x3C
#define PCI_INT_PIN_REG_OFFSET 0x3D
#define PCI_MIN_TIME_REG_OFFSET 0x3E
#define PCI_MAX_TIME_REG_OFFSET 0x3F

#define PCI_VENDOR_ID_NO_DEVICE 0xFFFF
typedef struct {
     uint16_t vendor_id;           // 0x00 - Vendor ID = FFFFh if not 'a valid entry'
     uint16_t device_id;           // 0x02 - Device ID
     uint16_t command;             // 0x04 - Command
     uint16_t status;              // 0x06 - Status
     uint8_t  revision_id;         // 0x08 - Revision ID
     uint8_t  prog_interface;      // 0x09 - Programming interface
     uint8_t  dev_sub_class;       // 0x0A - Sub class
     uint8_t  dev_class;           // 0x0B - Class code
     uint8_t  cache_line_size;     // 0x0C - Cache Line Size
     uint8_t  latency_timer;       // 0x0D - Latency Timer
     uint8_t  header_type;         // 0x0E - Header Type (0,1,2, if bit 7 set, multifunction device)
     uint8_t  self_test;           // 0x0F - Built in self test result
     uint32_t base_addr0;          // 0x10 - This one for OHCI, EHCI, and xHCI
     uint32_t base_addr1;          // 0x14 - 
     uint32_t base_addr2;          // 0x18 - 
     uint32_t base_addr3;          // 0x1C
     uint32_t base_addr4;          // 0x20 - This one for UHCI
     uint32_t base_addr5;          // 0x24 -
     uint32_t card_bus;            // 0x28 - CardBus CIS pointer
     uint16_t sub_sys_vendor_id;   // 0x2C - Subsystem vendor id
     uint16_t sub_sys_id;          // 0x2E - SubSystem id
     uint32_t rom_base_addr;       // 0x30 - Expansion ROM base address
     uint8_t  caps_off;            // 0x34 - Capabilities offset within this space
     uint8_t  resv0[3];            // 0x35 - reserved
     uint32_t resv1;               // 0x38 - reserved
     uint8_t  irq;                 // 0x3C - Interrupt line (0 = None, 1 = IRQ1, etc...)
     uint8_t  int_pin;             // 0x3D - Interrupt pin (0 = None, 1 = INTA, etc...)
     uint8_t  min_time;            // 0x3E - Minimum time bus master needs PCI bus ownership
     uint8_t  max_time;            // 0x3F - Maximum latency
     uint32_t dev_data[48];        // 0x40 - Varies by device.
} pci_device;

struct pci_address {
    uint32_t bus;
    uint32_t device;
    uint32_t func;
};

enum usb_hc {
    usb_hc_uhci = 0x0,
    usb_hc_ohci = 0x10,
    usb_hc_ehci = 0x20,
    usb_hc_xhci = 0x30
};

uint32_t pci_read_dword(struct pci_address* addr, uint8_t reg_offset);
uint16_t pci_read_word(struct pci_address* addr, uint8_t reg_offset);
uint8_t pci_read_byte(struct pci_address* addr, uint8_t reg_offset);

void pci_write_dword(struct pci_address* addr, uint8_t reg_offset, uint32_t value);
void pci_write_word(struct pci_address* addr, uint8_t reg_offset, uint16_t value);
void pci_write_byte(struct pci_address* addr, uint8_t reg_offset, uint8_t value);

bool pci_device_get_next(struct pci_address* addr, int16_t class_id, int16_t sub_class, pci_device* result);

#endif
