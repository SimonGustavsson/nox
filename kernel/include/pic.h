#include "stdint.h"
#include "pio.h"

// Handled by PIC1
#define PIC_IRQ_TIMER     0
#define PIC_IRQ_KEYBOARD  1
#define PIC_IRQ_SLAVE     2
#define PIC_IRQ_SERIAL2   3
#define PIC_IRQ_SERIAL1   4
#define PIC_IRQ_PARALLEL2 5
#define PIC_IRQ_DISKETTE  6
#define PIC_IRQ_PARALLEL1 7

// Handled by PIC2
#define PIC_IRQ_CMOSTIMER  0
#define PIC_IRQ_CGARETRACE 1
#define PIC_IRQ_AUX        4
#define PIC_IRQ_FPU        5
#define PIC_IRQ_HDC        6

// ICW1
#define ICW_1 0x11 // 000100001b - Enable init mode, send ICW4

// ICW1
#define PIC_ICW1_MASK_IC4  0x1  // 00000001b - Expect ICW4
#define PIC_ICW1_MASK_SNGL 0x2  // 00000010b - Signle/Cascaded
#define PIC_ICW1_MASK_ADI  0x4  // 00000100b - Call Addr Interval
#define PIC_ICW1_MASK_LTIM 0x8  // 00001000b - Operation Mode
#define PIC_ICW1_MASK_INIT 0x10 // 00010000b - Init Command

#define PIC_ICW1_IC4_EXPECT   1
#define PIC_ICW1_IC4_NO       0
#define PIC_ICW1_SNGL_YES     2
#define PIC_ICW1_SNGL_NO      0
#define PIC_ICW1_ADI_CALLINT4 4
#define PIC_ICW1_ADI_CALLINT8 0
#define PIC_ICW1_LTIM_LEVEL   8
#define PIC_ICW1_LTIM_EDGE    0
#define PIC_ICW1_INIT_YES     0x10
#define PIC_ICW1_INIT_NO      0

// OCW2
#define PIC_OCW2_MASK_L1     0x01 // 00000001
#define PIC_OCW2_MASK_L2     0x02 // 00000010
#define PIC_OCW2_MASK_L3     0x04 // 00000100
#define PIC_OCW2_MASK_EOI    0x20 // 00100000
#define PIC_OCW2_MASK_SL     0x40 // 01000000
#define PIC_OCW2_MASK_ROTATE 0x80 // 10000000

// OCW3
#define PIC_OCW3_MASK_RIS  0x01 // 00000001b
#define PIC_OCW3_MASK_RIR  0x02 // 00000010b
#define PIC_OCW3_MASK_MODE 0x04 // 00000100b
#define PIC_OCW3_MASK_SMM  0x20 // 00100000b
#define PIC_OCW3_MASK_ESMM 0x40 // 01000000b
#define PIC_OCW3_MASK_D7   0x80 // 10000000b

// ICW4
#define PIC_ICW4_MASK_UPM  0x1  // 00000001b - Mode
#define PIC_ICW4_MASK_AEOI 0x2  // 00000010b - Automatic EOI
#define PIC_ICW4_MASK_MS   0x4  // 00000100b - Buffer Type
#define PIC_ICW4_MASK_BUF  0x8  // 00001000b - Buffered Mode
#define PIC_ICW4_MASK_SFNM 0x10 // 00010000b - Special fully-nested mode

#define PIC_ICW4_UPM_86MODE          1 // 1b
#define PIC_ICW4_UPM_MMCSMODE        0
#define PIC_ICW4_AEOI_AUTOEOI        2 // 10b
#define PIC_ICW4_AEOI_NOAUTOEOI      0
#define PIC_ICW4_MS_BUFFEREDMASTER   4 // 100b
#define PIC_ICW4_MS_BUFFERSLAVE      0
#define PIC_ICW4_BUF_MODEYES         8 // 1000b
#define PIC_ICW4_BUF_MODENO          0
#define PIC_ICW4_SFNM_NESTED      0x10 // 10000b
#define PIC_ICW4_SFNM_NOTNESTED       0

#define PIC1_CTRL   0x20 // Write-Only
#define PIC1_STATUS 0x20 // Read-Only
#define PIC1_DATA   0x21 // Write-Only
#define PIC1_IMR    0x21 // Read-Only

#define PIC2_CTRL   0xA0 // Write-Only
#define PIC2_STATUS 0xA0 // Read-Only
#define PIC2_DATA   0xA1 // Write-Only
#define PIC2_IMR    0xA1 // Read-Only

#define IRQ_0 0x20                // IRQ 0-7 mapped to 0x20-0x27 - this is the PIT
#define IRQ_1 (IRQ_0 + 1)         // This is the Keyboard
#define IRQ_8 0x28                // IRQ 8-15 mapped to 0x28-0x36

void pic_initialize();
void pic_sendEOI(uint8_t irq);
void pic_sendEOI_toPic(uint8_t whichPic);
void pic_sendCommand(uint8_t pic, uint8_t cmd);
void pic_sendData(uint8_t pic, uint8_t data);
void pic_disableIRQ(uint8_t irq);
void pic_enableIRQ(uint8_t irq);
uint8_t pic_readData(uint8_t pic);

/*  Addr   IRQ      Desc
	0x000	0		Divide by 0
	0x004	1		Single step (Debugger)
	0x008	2		Non Maskable Interrupt (NMI) Pin
	0x00C	3		Breakpoint (Debugger)
	0x010	4		Overflow
	0x014	5		Bounds check
	0x018	6		Undefined Operation Code (OPCode) instruction
	0x01C	7		No coprocessor
	0x020	8		Double Fault
	0x024	9		Coprocessor Segment Overrun
	0x028	10		Invalid Task State Segment (TSS)
	0x02C	11		Segment Not Present
	0x030	12		Stack Segment Overrun
	0x034	13		General Protection Fault (GPF)
	0x038	14		Page Fault
	0x03C	15		Unassigned
	0x040	16		Coprocessor error
	0x044	17		Alignment Check (486+ Only)
	0x048	18		Machine Check (Pentium/586+ Only)
	0x05C	19-31	Reserved exceptions
	0x068 - 0x3FF	32-255	Interrupts free for software use
*/
