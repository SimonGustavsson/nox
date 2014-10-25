#ifndef PIO_H
#define PIO_H

#include <stdint.h>

// in port double (32-bit) __inline__ 
uint32_t inpd(uint16_t port);
uint16_t inpw(uint16_t port);
void outpd(uint16_t port, uint32_t data);
void outpw(uint16_t port, uint16_t data);

#endif