#include <types.h>
#include <pio.h>
#include <pci.h>

// -------------------------------------------------------------------------
// Forward declarations 
// -------------------------------------------------------------------------
static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg_offset, uint8_t len);
static void pci_write(uint8_t bus, uint8_t dev, uint8_t func, uint8_t port, uint8_t len, uint32_t value);

// -------------------------------------------------------------------------
// Exports
// -------------------------------------------------------------------------
bool pci_address_advance(struct pci_address* address)
{
    if(++address->func > MAX_FUNC_PER_PCI_BUS_DEV - 1) {
        address->func = 0;
        if(++address->device > MAX_PCI_BUS_DEV_NR - 1) {
            address->device = 0;
            if(++address->bus > MAX_PCI_BUS_NR - 1) {
                address->bus = 0;
                return false;
            }
        }
    }

    return true;
}

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

bool pci_device_get_next(struct pci_address* addr, int16_t class_id, int16_t sub_class, pci_device* result)
{
	uint32_t* res_device_ptr = (uint32_t*)(result);

	for(; addr->bus < MAX_PCI_BUS_NR; addr->bus++)
	{
		for(; addr->device < MAX_PCI_BUS_DEV_NR; addr->device++)
		{
			for(; addr->func < MAX_FUNC_PER_PCI_BUS_DEV; addr->func++)
			{
                if(pci_read_word(addr, PCI_VENDOR_ID_REG_OFFSET) == PCI_VENDOR_ID_NO_DEVICE)
                    continue;

                if(class_id != -1 && pci_read_byte(addr, PCI_DEV_CLASS_REG_OFFSET) != class_id)
                    continue;

                if(sub_class != -1 && pci_read_byte(addr, PCI_DEV_SUB_CLASS_REG_OFFSET) != sub_class)
                    continue;

				// Device found, read in the entire configuration space
				for (uint8_t i = 0; i < 64; i++)
					res_device_ptr[i] = pci_read_dword(addr, (i << 2));

				return true;
			}

			// Ensure we scan all functions on the next device
			addr->func = 0;
		}

		// Ensure we scan all devices on the next bus
		addr->device = 0;
	}

	return false; // No more devices

}

uint32_t pci_device_get_memory_size(struct pci_address* addr, uint32_t bar_offset)
{
    // Save the original BAR so we can restore it once we're done
    uint32_t orig_bar = pci_read_dword(addr, bar_offset);
    
    // Writing all 1s to the BAR register makes the device
    // write the size of the memory region it occupies into the BAR
    pci_write_dword(addr, bar_offset, 0xFFFFFFFF);

    // Read the size from the BAR
    uint32_t size = pci_read_dword(addr, bar_offset);

    // Reset the BAR to the original value
    pci_write_dword(addr, bar_offset, orig_bar);

    // The size is actually inversed, so NOT the entire thing,
    // ignoring bit 0 as it's only used to indicate whether the device
    // uses port I/O or is memory mapped
    return ~(size & ~1);
}

