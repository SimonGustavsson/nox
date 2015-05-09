#ifndef NOX_STRING_H
#define NOX_STRING_H

#include <types.h>

uint8_t nybble_to_ascii(uint8_t val);
void itoa(int number, char* buf);
size_t strlen(const char* str);
bool kstrcmp(char* a, char* b);

#endif

