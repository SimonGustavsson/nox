#include <stddef.h>
#include "stdint.h"
#include "idt.h"

void idt_install(idt_descriptor* idt)
{
  __asm("LIDT %0": /* no out */ : "m"(*idt));
}

void idt_remove()
{
  idt_install(NULL);
}

idt_descriptor idt_get()
{
  idt_descriptor addr;

  __asm("SIDT %0": /* no out */ : "m"(addr));

  return addr;
}

void idt_install_handler(uint8_t irq, uint32_t handler, gate_type type, uint8_t priv_level)
{
  idt_descriptor idtDescriptor = idt_get();

  idt_entry* idts = (idt_entry*)idtDescriptor.base;
  idt_entry* entry = &idts[irq];
  entry->offsetLow = (uint16_t)(handler & 0xFFFF);
  entry->offsetHigh = (uint16_t)((handler >> 16) & 0xFFFF);
  entry->selector = 0x08;
  entry->typeAttr.bits.present = 1;
  entry->typeAttr.bits.privLevel = priv_level;
  entry->typeAttr.bits.segment = 0;
  entry->typeAttr.bits.type = type;
}

void idt_remove_handler(uint8_t irq)
{
  // DO IT
}

void idt_enable_handler(uint8_t irq)
{
  idt_descriptor idtDescriptor = idt_get();

  idt_entry* idts = (idt_entry*)idtDescriptor.base;
  idt_entry* entry = &idts[irq];

  entry->typeAttr.bits.present = 1;
}

void idt_disableHandler(uint8_t irq)
{
  idt_descriptor idtDescriptor = idt_get();

  idt_entry* idts = (idt_entry*)idtDescriptor.base;
  idt_entry* entry = &idts[irq];

  entry->typeAttr.bits.present = 0;
}
