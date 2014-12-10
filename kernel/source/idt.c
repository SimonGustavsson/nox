#include <stddef.h>
#include "stdint.h"
#include "idt.h"

void idt_install(Idtd* idt)
{
	__asm("LIDT %0": /* no out */ : "m"(*idt));
}

void idt_remove()
{
    idt_install(NULL);
}

Idtd idt_get()
{
	Idtd addr;

	__asm("SIDT %0": /* no out */ : "m"(addr));

	return addr;
}

void idt_installHandler(uint8_t irq, uint32_t handler, GateType type, uint8_t privLevel) 
{
	Idtd idtDescriptor = idt_get();

    IdtEntry* idts = (IdtEntry*)idtDescriptor.base;
	IdtEntry* entry = &idts[irq];
	entry->offsetLow = (uint16_t)(handler & 0xFFFF);
	entry->offsetHigh = (uint16_t)((handler >> 16) & 0xFFFF);
    entry->selector = (privLevel & 0x3) | 0 /* TI */ | 0x08 << 3;
	entry->typeAttr.bits.present = 1;
	entry->typeAttr.bits.privLevel = privLevel;
	entry->typeAttr.bits.segment = 0;
	entry->typeAttr.bits.type = type;

     __asm("xchg %bx, %bx");
}

void idt_removeHandler(uint8_t irq)
{
    // DO IT
}

void idt_enableHandler(uint8_t irq)
{
	Idtd idtDescriptor = idt_get();

    IdtEntry* idts = (IdtEntry*)idtDescriptor.base;
	IdtEntry* entry = &idts[irq];

	entry->typeAttr.bits.present = 1;
}

void idt_disableHandler(uint8_t irq)
{
	Idtd idtDescriptor = idt_get();

    IdtEntry* idts = (IdtEntry*)idtDescriptor.base;
	IdtEntry* entry = &idts[irq];

	entry->typeAttr.bits.present = 0;
}
