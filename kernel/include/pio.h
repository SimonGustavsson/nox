#ifndef PIO_H
#define PIO_H

#include <stdint.h>

// in port double (32-bit) __inline__ 
uint32_t ind(uint16_t port);
uint16_t inw(uint16_t port);
uint8_t inb(uint16_t port);

void outd(uint16_t port, uint32_t data);
void outw(uint16_t port, uint16_t data);
void outb(uint16_t port, uint8_t data);

#endif
