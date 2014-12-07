#ifndef PCI_H
#define PCI_H

#if !defined(__cplusplus)
#include <stdbool.h> // C Does not have bool :(
#endif

#include <stdint.h>

#define  PCI_ADDR     0x0CF8
#define  PCI_DATA     0x0CFC

#define MAX_PCI_BUS_NR 256
#define MAX_PCI_BUS_DEV_NR 32
#define MAX_FUNC_PER_PCI_BUS_DEV 8

#define USB_CLASS_CODE (0xC)
#define USB_SUBCLASS_CODE (0x03)

typedef struct {
  uint16_t vendorid;         // Vendor ID = FFFFh if not 'a valid entry'
  uint16_t deviceId;         // Device ID
  uint16_t command;          // Command
  uint16_t status;           // Status
  uint8_t  revisionId;            // Revision ID
  uint8_t  progInterface;      //
  uint8_t  devSubClass;        // 
  uint8_t  devClass;           //
  uint8_t  cacheLineSize;      // Cache Line Size
  uint8_t  latencyTimer;       // Latency Timer
  uint8_t  headerType;          // Header Type (0,1,2, if bit 7 set, multifunction device)
  uint8_t  selfTest;
  uint32_t baseAddr0;            // This one for OHCI, EHCI, and xHCI
  uint32_t baseAddr1;            //
  uint32_t baseAddr2;            //
  uint32_t baseAddr3;            //
  uint32_t baseAddr4;            // This one for UHCI
  uint32_t baseAddr5;            //
  uint32_t cardBus;          //
  uint16_t subSysVendorid;        //
  uint16_t subSysId;         //
  uint32_t romBaseAddr;          //
  uint8_t  capsOff;          //
  uint8_t  resv0[3];         //
  uint32_t resv1;            //
  uint8_t  irq;              //
  uint8_t  intPin;           //
  uint8_t  minTime;         //
  uint8_t  maxTime;         //
  uint32_t devData[48];       // varies by device.
} PCI_DEVICE;

typedef struct {
	uint32_t bus;
	uint32_t device;
	uint32_t func;
} PCI_ADDRESS;

uint32_t read_pci(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len);
void write_pci(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len, uint32_t value);
bool getNextUsbController(PCI_ADDRESS* addr, PCI_DEVICE* resDevice);

#endif