// idt.h
#include <stdint.h>
// Gate Types for 3:0 of typeAttr in idt_entry_t
typedef enum {
	GateType_Task32      = 0x5,
	GateType_Interrupt16 = 0x6,
	GateType_Trap16      = 0x7,
	GateType_Interrupt32 = 0xE,
	GateType_Trap32      = 0xF	
} GateType;

typedef struct {
	uint16_t limit; // Length of IDT in bytes - 1
	uint32_t base; // Linear start address
} Idtd;

typedef union {
	uint8_t raw;
	
	struct {
	    uint8_t        present:   1; // 0 for unused interrupts
	    uint8_t        privLevel: 2; // Gate call protection (minimum caller level)
	    uint8_t        segment:   1; // Storage segment (0 for interrupt gates)
	    GateType       type:      4;
	} bits;
} IdtTypeAttr;

typedef struct {
	uint16_t       offsetLow;  // 31:16 of offset
	uint16_t       selector;   // Code segment selector in GDT/LDT
	uint8_t        zero;
	IdtTypeAttr    typeAttr;
	uint16_t       offsetHigh; // 15:0 of offset
} IdtEntry;

Idtd* idt_get();
void idt_install(Idtd* idt);
void idt_remove();
void idt_installHandler(uint8_t irq, uint32_t handler, GateType type, uint8_t privLevel);
void idt_enableHandler(uint8_t irq);
void idt_disableHandler(uint8_t irq);
