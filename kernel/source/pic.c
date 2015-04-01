// PIC.c
#include "stdint.h"
#include "pic.h"

void pic_send_command(uint8_t pic, uint8_t cmd)
{
	if(pic > 1) return; // Only 2 PICs supported
	OUTB(pic == 0 ? PIC1_CTRL : PIC2_CTRL, cmd);
}

void pic_send_data(uint8_t pic, uint8_t data)
{
	if(pic > 1) return; // Only 2 PICs supported

	OUTB(pic == 0 ? PIC1_DATA : PIC2_DATA, data);
}

uint8_t pic_read_data(uint8_t pic)
{
	if(pic > 1) return 0; // Only supports 2 PICs

	return INB(pic == 0 ? PIC1_DATA : PIC2_DATA);
}

void pic_send_eoi_to_pic(uint8_t whichPic)
{
    pic_send_command(whichPic, PIC_OCW2_MASK_EOI);

    // If resetting the slave PIC, we need to
    // do the master too
    if (whichPic == 1) {
      pic_send_command(0, PIC_OCW2_MASK_EOI);
    }
}

void pic_send_eoi(uint8_t irq)
{
	if(irq >= 8) {
		pic_send_eoi_to_pic(1);
  }
  else {
		pic_send_eoi_to_pic(0);
  }
}

void pic_enable_irq_on_pic(uint8_t whichPic, uint8_t irqBitIndex) {
  uint8_t originalValue = pic_read_data(whichPic);
  uint8_t newValue = originalValue & ~(1 << irqBitIndex);
  pic_send_data(whichPic, newValue);
}

void pic_enable_irq(uint8_t irq) {
	if(irq >= 8) {

    // IRQ8 is the zeroeth bit on the slave
    // PIC
    pic_enable_irq_on_pic(1, irq - 8);

  }
  else {
    pic_enable_irq_on_pic(0, irq);
  }
}

void pic_disable_irq_on_pic(uint8_t whichPic, uint8_t irqBitIndex) {
  uint8_t originalValue = pic_read_data(whichPic);
  uint8_t newValue = originalValue | (1 << irqBitIndex);
  pic_send_data(whichPic, newValue);
}

void pic_disable_irq(uint8_t irq) {
	if(irq >= 8) {

    // IRQ8 is the zeroeth bit on the slave
    // PIC
    pic_disable_irq_on_pic(1, irq - 8);

  }
  else {
    pic_disable_irq_on_pic(0, irq);
  }
}

void pic_initialize_inner(uint8_t base0, uint8_t base1)
{
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

  // Disable all interrupts by default, except for IRQ2 which
  // is used by the slave to send IRQs via the master
	pic_sendData(0, 0xFF & ~(1 << PIC_IRQ_SLAVE));
	pic_sendData(1, 0xFF);
}

void pic_initialize()
{
    pic_initialize_inner(IRQ_0, IRQ_8);
}

