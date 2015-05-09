#include <types.h>
#include <pio.h>
#include <pci.h>

// read from the pci config space
uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len)
{
	uint32_t ret;

	const uint32_t val = 0x80000000 |
		(bus << 16) |
		(dev << 11) |
		(func << 8) |
		(port & 0xFC);

	OUTD(PCI_ADDR, val);

	ret = IND(PCI_DATA + (port & 0x3));

	ret &= (0xFFFFFFFF >> ((4-len) * 8));

	return ret;
}

// write to the pci config space
void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len, uint32_t value)
{
	uint32_t val = 0x80000000 |
		(bus << 16) |
		(dev << 11) |
		(func << 8) |
		(port & 0xFC);

	OUTD(PCI_ADDR, val);

	// get current value
	val = IND(PCI_DATA+(port & 0x3));

	// mask out new section
	if (len != 4)
		val &= (0xFFFFFFFF << (len * 8));
	else
		val = 0;         // vc++/processor does not allow shift counts of 32

	val |= value;

	OUTD(PCI_DATA + (port & 0x3), val);
}

// addr is the adress to start scanning at, and will be the address when the function returns
bool pci_get_next_usbhc(pci_address* addr, pci_device* resDevice)
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
					res_device_ptr[i] = pci_read(addr->bus, addr->device, addr->func, (i << 2), sizeof(uint32_t));

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
