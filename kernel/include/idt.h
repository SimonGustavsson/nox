// idt.h
#include <stdint.h>

#define PACKED __attribute__((__packed__))

// Gate Types for 3:0 of typeAttr in idt_entry_t
typedef enum {
	gate_type_task32      = 0x5,
	gate_type_interrupt16 = 0x6,
	gate_type_trap16      = 0x7,
	gate_type_interrupt32 = 0xE,
    gate_type_trap32      = 0xF	
} gate_type;

typedef struct PACKED {
	uint16_t limit; // Length of IDT in bytes - 1
	uint32_t base; // Linear start address
} idt_descriptor;

typedef union {
	uint8_t raw;
	
	struct PACKED {
	    uint8_t        type:      4;
	    uint8_t        segment:   1; // Storage segment (0 for interrupt gates)
	    uint8_t        privLevel: 2; // Gate call protection (minimum caller level)
	    uint8_t        present:   1; // 0 for unused interrupts
	} bits;
} idt_type_attribute;

typedef struct PACKED {
	uint16_t              offsetLow; // 15:0 of offset
	uint16_t              selector;   // Code segment selector in GDT/LDT
	uint8_t               zero;
	idt_type_attribute    typeAttr;
	uint16_t              offsetHigh;  // 31:16 of offset
} idt_entry;

idt_descriptor idt_get();
void idt_install(idt_descriptor* idt);
void idt_remove();
void idt_install_handler(uint8_t irq, uint32_t handler, gate_type type, uint8_t priv_level);
void idt_enable_handler(uint8_t irq);
void idt_disable_handler(uint8_t irq);
