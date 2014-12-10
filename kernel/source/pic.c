// PIC.c
#include "stdint.h"
#include "pic.h"

void pic_sendCommand(uint8_t pic, uint8_t cmd)
{
	if(pic > 1) return; // Only 2 PICs supported
	outb(pic == 0 ? PIC1_CTRL : PIC2_CTRL, cmd);
}

void pic_sendData(uint8_t pic, uint8_t data)
{
	if(pic > 1) return; // Only 2 PICs supported

	outb(pic == 0 ? PIC1_DATA : PIC2_DATA, data);
}

uint8_t pic_readData(uint8_t pic)
{
	if(pic > 1) return 0; // Only supports 2 PICs

	return inb(pic == 0 ? PIC1_DATA : PIC2_DATA);
}

void pic_sendEOI(uint8_t irq)
{
	if(irq >= 8)
		pic_sendCommand(1, PIC_OCW2_MASK_EOI);

	pic_sendCommand(0, PIC_OCW2_MASK_EOI);
}

void pic_initialize_inner(uint8_t base0, uint8_t base1)
{
	// Normally reading the data register returns the IMR register.
	// Save it before we start sending data so that we can restore it once
	// the initialization is complete
	uint8_t pic0Data = pic_readData(0);
	uint8_t pic1Data = pic_readData(1);

	// Send ICW1 - To start initialization sequence in cascade mode which
	//             causes the PIC to wait for 3 init words on the data channel
	pic_sendCommand(0, PIC_ICW1_MASK_INIT + PIC_ICW1_IC4_EXPECT);
	pic_sendCommand(1, PIC_ICW1_MASK_INIT + PIC_ICW1_IC4_EXPECT);

	// Init word 1: Set base addr of IRQs
	pic_sendData(0, base0);
	pic_sendData(1, base1);

	// Init word 2: Tell the PIC that there's a slave PIC at IRQ2
	pic_sendData(0, 0x4);
	pic_sendData(1, 0x2); // Tell the slave it's cascade identity (decimal)

	// Init word3: Enable i86 Mode
	pic_sendData(0, PIC_ICW4_UPM_86MODE);
	pic_sendData(1, PIC_ICW4_UPM_86MODE);

	pic_sendData(0, ~(1<<2));         /* Mask all interrupts but IRQ2 */
	pic_sendData(1, 0xFF); 

	// Restore saved data values
	pic_sendData(0, pic0Data);
	pic_sendData(0, pic1Data);
}

void pic_initialize()
{
    pic_initialize_inner(IRQ_0, IRQ_8);
}

