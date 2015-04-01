// pio.c 
#include <stdint.h> 
#include "pio.h" 

// in port double (32-bit) __inline__ 
uint32_t IND(uint16_t port) 
{
    uint32_t rv;
    __asm__ __volatile__ ("inl %1, %0"
            : "=a" (rv)
            : "dN" (port));
    return rv;
}

uint16_t INW(uint16_t port)
{
    uint16_t rv;
    __asm__ __volatile__ ("in %1, %0"
            : "=a" (rv)
            : "dN" (port));

    return rv;
}

uint8_t INB(uint16_t port)
{
    uint8_t rv;
    __asm__ __volatile__ ("in %1, %0"
            : "=a" (rv)
            : "dN" (port));

    return rv;
}

void OUTD(uint16_t port, uint32_t data) {
    __asm__ __volatile__ ("outl %1, %0"
            :
            : "dN" (port),
            "a" (data));
}

void OUTW(uint16_t port, uint16_t data)
{
    __asm__ __volatile__ ("out %1, %0"
            :
            : "dN" (port),
            "a" (data));
}

void OUTB(uint16_t port, uint8_t data)
{
    __asm__ __volatile__ ("out %1, %0"
            :
            : "dN" (port),
            "a" (data));
}
