// PIC.c
#include "stdint.h"
#include "pic.h"

void pic_remapIrq(uint8_t base0, uint8_t base1)
{
	// Begin init by sending ICW1
	uint8_t icw = 0;
	icw = (icw & ~PIC_ICW1_MASK_INIT) | PIC_ICW1_INIT_YES;
	icw = (icw & ~PIC_ICW1_MASK_IC4) | PIC_ICW1_IC4_EXPECT;

	pic_sendCommand(0, icw);
	pic_sendCommand(1, icw);

	// Set base addr of IRQs
	pic_sendData(0, base0);
	pic_sendData(1, base1);

	// Connect master with slave on IR 2
	// Note: Master IR is in binary whereas slave is in decimal
	pic_sendData(0, 0x4);
	pic_sendData(1, 0x2);

	// Enable i86 Mode
	icw = (icw & ~PIC_ICW4_MASK_UPM) | PIC_ICW4_UPM_86MODE;

	pic_sendCommand(0, icw);
	pic_sendCommand(1, icw);
}
