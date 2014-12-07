#include <stddef.h>
#include "stdint.h"
#include "idt.h"

void idt_install(Idtd* idt)
{
	uint32_t addr = (uint32_t)idt;
	__asm("LIDT %0": /* no out */ : "m"(addr));
}

void idt_remove()
{
	uint32_t addr = 0;
	__asm("LIDT %0": /* no out */ : "m"(addr));
}

Idtd* idt_get()
{
	uint32_t addr;

	__asm("SIDT %0": "=m"(addr) : /* no input */);

	return (Idtd*)addr;
}

void idt_installHandler(uint8_t irq, uint32_t handler, GateType type, uint8_t privLevel) 
{
	Idtd* idtDescriptor = idt_get();
	if(idtDescriptor == NULL)
	{
		// Invalid call to idt_installHandler before idt has been installed
	}

    IdtEntry* idts = (IdtEntry*)idtDescriptor->base;
	IdtEntry* entry = &idts[irq];
	entry->offsetLow = (uint16_t)(handler & 0xFFFF);
	entry->offsetHigh = (uint16_t)((handler >> 16) & 0xFFFF);
	entry->typeAttr.bits.present = 1;
	entry->typeAttr.bits.privLevel = privLevel;
	entry->typeAttr.bits.segment = 0;
	entry->typeAttr.bits.type = type;
}

void idt_removeHandler(uint8_t irq)
{
	Idtd* idt = idt_get();
	if(idt == NULL)
	{
		// Invalid call to removeHandler before idt has been installed
	}

	uint32_t* entry = (uint32_t*)&idt[irq];

	*entry = 0;
	*(entry + 1) = 0;
}

void idt_enableHandler(uint8_t irq)
{
	Idtd* idtDescriptor = idt_get();
	if(idtDescriptor == NULL)
	{
		// Invalid call to idt_installHandler before idt has been installed
	}

    IdtEntry* idts = (IdtEntry*)idtDescriptor->base;
	IdtEntry* entry = &idts[irq];

	entry->typeAttr.bits.present = 1;
}

void idt_disableHandler(uint8_t irq)
{
	Idtd* idtDescriptor = idt_get();
	if(idtDescriptor == NULL)
	{
		// Invalid call to idt_installHandler before idt has been installed
	}

    IdtEntry* idts = (IdtEntry*)idtDescriptor->base;
	IdtEntry* entry = &idts[irq];

	entry->typeAttr.bits.present = 0;
}
