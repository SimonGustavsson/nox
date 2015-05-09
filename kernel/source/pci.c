#include <stdbool.h> // C Does not have bool :(
#include <stddef.h>
#include <stdint.h>

#include "pio.h"
#include "pci.h"

// -------------------------------------------------------------------------
// Forward declarations 
// -------------------------------------------------------------------------
static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg_offset, uint8_t len);
static void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len, uint32_t value);

// -------------------------------------------------------------------------
// Exports
// -------------------------------------------------------------------------
uint32_t pci_read_dword(struct pci_address* addr, uint8_t reg_offset)
{
    return pci_read(addr->bus, addr->device, addr->func, reg_offset, sizeof(uint32_t));
}

uint16_t pci_read_word(struct pci_address* addr, uint8_t reg_offset)
{
    return (uint16_t)pci_read(addr->bus, addr->device, addr->func, reg_offset, sizeof(uint16_t));
}

uint8_t pci_read_byte(struct pci_address* addr, uint8_t reg_offset)
{
    return (uint8_t)pci_read(addr->bus, addr->device, addr->func, reg_offset, sizeof(uint8_t)); 
}

void pci_write_dword(struct pci_address* addr, uint8_t reg_offset, uint32_t value)
{
    pci_write(addr->bus, addr->device, addr->func, reg_offset, sizeof(uint32_t), value);
}

void pci_write_word(struct pci_address* addr, uint8_t reg_offset, uint16_t value)
{
    pci_write(addr->bus, addr->device, addr->func, reg_offset, sizeof(uint16_t), value);
}

void pci_write_byte(struct pci_address* addr, uint8_t reg_offset, uint8_t value)
{
    pci_write(addr->bus, addr->device, addr->func, reg_offset, sizeof(uint8_t), value);
}

// -------------------------------------------------------------------------
// Privates
// -------------------------------------------------------------------------
static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg_offset, uint8_t len)
{
	uint32_t ret;

	const uint32_t val = 0x80000000 |
		(bus << 16) |
		(dev << 11) |
		(func << 8) |
		(reg_offset & 0xFC);


	OUTD(PCI_ADDR_IO_PORT, val);

	ret = IND(PCI_DATA_IO_PORT + (reg_offset & 0x3));

	ret &= (0xFFFFFFFF >> ((4-len) * 8));

	return ret;
}

// write to the pci config space
static void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg_offset, uint8_t len, uint32_t value)
{
	uint32_t val = 0x80000000 |
		(bus << 16) |
		(dev << 11) |
		(func << 8) |
		(reg_offset & 0xFC);

	OUTD(PCI_ADDR_IO_PORT, val);

	// get current value
	val = IND(PCI_DATA_IO_PORT + (reg_offset & 0x3));
	
	// mask out new section
	if (len != 4)
		val &= (0xFFFFFFFF << (len * 8));
	else
		val = 0;         // vc++/processor does not allow shift counts of 32

	val |= value;

	OUTD(PCI_DATA_IO_PORT + (reg_offset & 0x3), val);
}

// addr is the adress to start scanning at, and will be the address when the function returns
bool pci_get_next_usbhc(struct pci_address* addr, pci_device* resDevice)
{
	uint32_t* res_device_ptr = (uint32_t*)(resDevice);

	for(; addr->bus < MAX_PCI_BUS_NR; addr->bus++)
	{
		for(; addr->device < MAX_PCI_BUS_DEV_NR; addr->device++)
		{
			for(; addr->func < MAX_FUNC_PER_PCI_BUS_DEV; addr->func++)
			{
				// Make sure a device exists here
				if(pci_read(addr->bus, addr->device, addr->func, 0, sizeof(uint16_t)) == 0xFFFF)
					continue; // No device, continue the search

				// read in the 256 bytes (64 dwords)
				// TODO: This fills the result thing with loads of data, that might not
				// be a valid device, if we end up never finding a device, we end up returning garbage
				for (uint32_t i = 0; i < 64; i++)
					res_device_ptr[i] = pci_read_dword(addr, (i << 2));

				if(resDevice->dev_class != USB_CLASS_CODE || resDevice->dev_sub_class != USB_SUBCLASS_CODE)
					continue; // Not a usb device

				// We found a usb device!
				return true;
			}

			// Ensure we scan all functions on the next device
			addr->func = 0;
		}

		// Ensure we scan all devices on the next bus
		addr->device = 0;
	}

	// Looped through all devices and found nothing, boo?
	return false;
}
