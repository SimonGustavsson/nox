#include <stddef.h>
#include "stdint.h"
#include "idt.h"

void idt_install(idt_t* idt)
{
	uint32_t addr = (uint32_t)idt;
	__asm("LIDT %0": /* no out */ : "m"(addr));
}

void idt_remove()
{
	uint32_t addr = 0;
	__asm("LIDT %0": /* no out */ : "m"(addr));
}

idt_t* idt_get()
{
	uint32_t addr;

	__asm("SIDT %0": "=m"(addr) : /* no input */);

	return (idt_t*)addr;
}

void idt_installHandler(uint8_t irq, uint32_t handler, idt_gateType_t type, uint8_t privLevel) 
{
	idt_t* idtDescriptor = idt_get();
	if(idtDescriptor == NULL)
	{
		// Invalid call to idt_installHandler before idt has been installed
	}

    idt_entry_t* idts = (idt_entry_t*)idtDescriptor->base;
	idt_entry_t* entry = &idts[irq];
	entry->offsetLow = (uint16_t)(handler & 0xFFFF);
	entry->offsetHigh = (uint16_t)((handler >> 16) & 0xFFFF);
	entry->typeAttr.bits.present = 1;
	entry->typeAttr.bits.privLevel = privLevel;
	entry->typeAttr.bits.segment = 0;
	entry->typeAttr.bits.type = type;
}

void idt_removeHandler(uint8_t irq)
{
	idt_t* idt = idt_get();
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
	idt_t* idtDescriptor = idt_get();
	if(idtDescriptor == NULL)
	{
		// Invalid call to idt_installHandler before idt has been installed
	}

    idt_entry_t* idts = (idt_entry_t*)idtDescriptor->base;
	idt_entry_t* entry = &idts[irq];

	entry->typeAttr.bits.present = 1;
}

void idt_disableHandler(uint8_t irq)
{
	idt_t* idtDescriptor = idt_get();
	if(idtDescriptor == NULL)
	{
		// Invalid call to idt_installHandler before idt has been installed
	}

    idt_entry_t* idts = (idt_entry_t*)idtDescriptor->base;
	idt_entry_t* entry = &idts[irq];

	entry->typeAttr.bits.present = 0;
}
