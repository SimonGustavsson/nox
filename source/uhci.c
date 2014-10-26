#include "uhci.h"
#include "pio.h"

int uhci_detect_root(uint16_t baseAddr, bool ioAddr)
{
	if(!ioAddr)
		return -1; // Not currently supported

	for(int i = 0; i < 5; i++)
	{
		outpw(baseAddr + UHCI_CMD_OFFSET, 0x4);

		wait(11);

		outpw(baseAddr + UHCI_CMD_OFFSET, 0x0);
	}

	if(inpw(baseAddr + UHCI_CMD_OFFSET) != 0x0)
		return -2; // CMD Reg does not have its default value of 0

	if(inpw(baseAddr + UHCI_STS_OFFSET) != 0x20)
		return -3; // Status Reg does not have its default value of 0x20

	// Clear out status reg (it's WC)
	outpw(baseAddr + UHCI_STS_OFFSET, 0xFF);

	if(inpw(baseAddr + UHCI_SOFMOD_OFFSET) != 0x40)
		return -4; // Start of Frame register does not have its default value of 0x40

	// Uf we set but 1, the controller should reset it to 0
	outpw(baseAddr + UHCI_CMD_OFFSET, 0x2);

	wait(42); // arbitrary wait

	if(inpw(baseAddr + UHCI_CMD_OFFSET) & 0x2)
		return -5; // Controller did not reset bit :(

	return 0; // Looks good
}
