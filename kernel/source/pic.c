// PIC.c
#include "stdint.h"
#include "pic.h"

#define ICW_1 0x11 // 000100001b - Enable init mode, send ICW4

// IO Ports for interracting with the PIC
#define PIC1_IO_PORT_CTRL   0x20 // Write-Only
#define PIC1_IO_PORT_STATUS 0x20 // Read-Only
#define PIC1_IO_PORT_DATA   0x21 // Write-Only
#define PIC1_IO_PORT_IMR    0x21 // Read-Only

#define PIC2_IO_PORT_CTRL   0xA0 // Write-Only
#define PIC2_IO_PORT_STATUS 0xA0 // Read-Only
#define PIC2_IO_PORT_DATA   0xA1 // Write-Only
#define PIC2_IO_PORTIMR     0xA1 // Read-Only

// ICW1
enum pic_icw1_mask {
    pic_icw1_mask_ic4  = 0x1, // 00000001b - Expect ICW4 
    pic_icw1_mask_sngl = 0x2, // 00000010b - Signle/Cascaded 
    pic_icw1_mask_adi  = 0x4, // 00000100b - Call Addr Interval 
    pic_icw1_mask_ltim = 0x8, // 00001000b - Operation Mode 
    pic_icw1_mask_init = 0x10 // 00010000b - Init Command 
};

// ICW1
enum pic_icw1_icw4 {
    pic_icw1_ic4_no = 0,
    pic_icw1_ic4_expect = 1
};
enum pic_icw1_sngl {
    pic_icw1_sngl_no = 0,
    pic_icw1_sngl_yes = 2
};

enum pic_icw1_adi {
    pic_icw1_adi_callint8 = 0,
    pic_icw1_adi_callint4 = 4
};

enum pic_icw1_ltim {
    pic_icw1_ltim_edge  = 0,
    pic_icw1_ltim_level = 8
};

enum pic_icw1_init {
    pic_icw1_init_no  = 0x0,
    pic_icw1_init_yes = 0x10
};

enum pic_ocw2_mask {
    pic_ocw2_mask_l1     = 0x01, // 00000001
    pic_ocw2_mask_l2     = 0x02, // 00000010
    pic_ocw2_mask_l3     = 0x04, // 00000100
    pic_ocw2_mask_eoi    = 0x20,  // 00100000
    pic_ocw2_mask_sl     = 0x40, // 01000000
    pic_ocw2_mask_rotate = 0x80  // 10000000
};

// OCW3
enum pic_ocw3_mask {
    pic_ocw3_mask_ris  = 0x01, // 00000001b
    pic_ocw3_mask_rir  = 0x02, // 00000010b
    pic_ocw3_mask_mode = 0x04, // 00000100b
    pic_ocw3_mask_smm  = 0x20, // 00100000b
    pic_ocw3_mask_esmm = 0x40, // 01000000b
    pic_ocw3_mask_d7   = 0x80  // 10000000b
};

// ICW4
enum pic_icw4_mask {
    pic_icw4_mask_upm  = 0x1,  // 00000001b - Mode
    pic_icw4_mask_aeoi = 0x2,  // 00000010b - Automatic EOI
    pic_icw4_mask_ms   = 0x4,  // 00000100b - Buffer Type
    pic_icw4_mask_buf  = 0x8,  // 00001000b - Buffered Mode
    pic_icw4_mask_sfnm = 0x10  // 00010000b - Special fully-nested mode
};
enum pic_icw4_upm {
    pic_icw4_upm_mmcsmode = 0,    // 1b
    pic_icw4_upm_86mode   = 1
};
enum pic_icw4_auto_eoi {
    pic_icw4_auto_eoi_no = 0,
    pic_icw4_auto_eoi_yes = 2     // 100b
};
enum pic_icw4_ms {
    pic_icw4_ms_buffer_slave = 0,
    pic_icw4_ms_buffer_master = 4 // 100b
};
enum pic_icw4_buf_mode {
    pic_icw4_buf_mode_no = 0,
    pic_icw4_buf_mode_yes = 8     // 1000b
};
enum pic_icw4_sfnm {
    pic_icw4_sfnm_notnested = 0,
    pic_icw4_sfnm_nested = 10     // 10000b
};

// -------------------------------------------------------------------------
// Forward declaractions
// -------------------------------------------------------------------------
static void pic_send_eoi_to_pic(uint8_t which_pic);
static void pic_enable_irq_on_pic(uint8_t which_pic, uint8_t irq_bit_index);
static void pic_disable_irq_on_pic(uint8_t which_pic, uint8_t irq_bit_index);
static void pic_init_inner(uint8_t irq0, uint8_t irq8);

// -------------------------------------------------------------------------
// Extern
// -------------------------------------------------------------------------
void pic_send_eoi(uint8_t irq)
{
    if(irq >= 8)
        pic_send_eoi_to_pic(1);
    else
        pic_send_eoi_to_pic(0);
}

void pic_enable_irq(uint8_t irq)
{
	if(irq >= 8)
        pic_enable_irq_on_pic(1, irq - 8); // IRQ8 is the zeroeth bit on the slave PIC
    else
        pic_enable_irq_on_pic(0, irq);
}

void pic_disable_irq(uint8_t irq)
{
    if(irq >= 8)
        pic_disable_irq_on_pic(1, irq - 8); // IRQ8 is the zeroeth bit on the slave PIC
    else
        pic_disable_irq_on_pic(0, irq);
}

void pic_init()
{
    pic_init_inner(IRQ_0, IRQ_8);
}

// -------------------------------------------------------------------------
// Private 
// -------------------------------------------------------------------------
static void pic_send_command(uint8_t pic, uint8_t cmd)
{
	if(pic > 1) return; // Only 2 PICs supported

    OUTB(pic == 0 ? PIC1_IO_PORT_CTRL : PIC2_IO_PORT_CTRL, cmd);
}

static void pic_send_data(uint8_t pic, uint8_t data)
{
	if(pic > 1) return; // Only 2 PICs supported

	OUTB(pic == 0 ? PIC1_IO_PORT_DATA : PIC2_IO_PORT_DATA, data);
}

static uint8_t pic_read_data(uint8_t pic)
{
	if(pic > 1) return 0; // Only supports 2 PICs

	return INB(pic == 0 ? PIC1_IO_PORT_DATA : PIC2_IO_PORT_DATA);
}

static void pic_send_eoi_to_pic(uint8_t which_pic)
{
    pic_send_command(which_pic, pic_ocw2_mask_eoi);

    // If resetting the slave PIC, we need to
    // do the master too
    if (which_pic == 1)
      pic_send_command(0, pic_ocw2_mask_eoi);
}

static void pic_enable_irq_on_pic(uint8_t which_pic, uint8_t irq_bit_index)
{
    uint8_t original_value = pic_read_data(which_pic);
    uint8_t new_value = original_value & ~(1 << irq_bit_index);
    pic_send_data(which_pic, new_value);
}

static void pic_disable_irq_on_pic(uint8_t which_pic, uint8_t irqBitIndex)
{
    uint8_t original_value = pic_read_data(which_pic);
    uint8_t new_value = original_value | (1 << irqBitIndex);
    pic_send_data(which_pic, new_value);
}

static void pic_init_inner(uint8_t base0, uint8_t base1)
{
	// Send ICW1 - To start initialization sequence in cascade mode which
	//             causes the PIC to wait for 3 init words on the data channel
	pic_send_command(0, pic_icw1_mask_init + pic_icw1_ic4_expect);
	pic_send_command(1, pic_icw1_mask_init + pic_icw1_ic4_expect);

	// Init word 1: Set base addr of IRQs
	pic_send_data(0, base0);
	pic_send_data(1, base1);

	// Init word 2: Tell the PIC that there's a slave PIC at IRQ2
	pic_send_data(0, 0x4);
	pic_send_data(1, 0x2); // Tell the slave it's cascade identity (decimal)

	// Init word3: Enable i86 Mode
	pic_send_data(0, pic_icw4_upm_86mode);
	pic_send_data(1, pic_icw4_upm_86mode);

    // Disable all interrupts by default, except for IRQ2 which
    // is used by the slave to send IRQs via the master
	pic_send_data(0, 0xFF & ~(1 << pic_irq_slave));
	pic_send_data(1, 0xFF);
}
