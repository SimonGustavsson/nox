#include "idt.h"

void idt_install(idt_t* idt)
{
	uint32_t addr = (uint32_t)idt;
	__asm("LIDT %addr");

	// or something like this?
	// (asm:output:input:clobberList)
	__asm("LIDT %0"::"r"(addr):);
}

void idt_remove()
{
	uint32_t addr = 0;
	__asm("LIDT %addr");
	//__asm("LIDT %0"::"r"(addr):); // (asm:output:input:clobberList)
}

idt_t* idt_get()
{
	uint32_t addr;
	__asm("SIDT %addr");
	//__asm("SIDT":"=r"(addr)::);

	return (idt_t*)addr;
}

void idt_installHandler(uint8_t irq, uint32_t handler, idt_gateType_t type, uint8_t privLevel) 
{
	idt_t* idt = idt_get();
	if(idt == NULL)
	{
		// Invalid call to idt_installHandler before idt has been installed
	}

	idt_entry_t entry = (idt_entry_t*)&idt[irq];
	entry->offsetLow = (uint16_t)(handler & 0xFFFF);
	entry->offsetHigh = (uint16_t)((handler >> 16) & 0xFFFF);
	entry->type.present = 1;
	entry->type.privLevel = privLevel;
	entry->type.segment = 0;
	entry->type.type = type;
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
	idt_t* idt = idt_get();
	if(idt == NULL)
	{
		// Invalid call to idt_enableHandler before idt has been installed
	}

	idt_entry_t entry = (idt_entry_t*)&idt[irq];

	entry->type.present = 1;
}

void idt_disableHandler(uint8_t irq)
{
	idt_t* idt = idt_get();
	if(idt == NULL)
	{
		// Invalid call to idt_disableHandler before idt has been installed
	}

	idt_entry_t entry = (idt_entry_t*)&idt[irq];

	entry->type.present = 0;
}
