#ifndef PIO_H
#define PIO_H

#include <stdint.h>

uint32_t IND(uint16_t port);
uint16_t INW(uint16_t port);
uint8_t INB(uint16_t port);

void OUTD(uint16_t port, uint32_t data);
void OUTW(uint16_t port, uint16_t data);
void OUTB(uint16_t port, uint8_t data);

#endif
