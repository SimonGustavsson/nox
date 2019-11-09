#ifndef NOX_STRING_H
#define NOX_STRING_H

#include <types.h>
#include <stdarg.h>

uint8_t nybble_to_ascii(uint8_t val);
char* uitoa(uint32_t number, char* buf);
uint32_t uatoi(const char* str);
char* itoa(int32_t number, char* buf);
size_t strlen(const char* str);
bool kstrcmp(const char* a, const char* b);
bool kstrcmp_n(const char* a, const char* b, size_t len);
char* kstrcpy_n(char* dest, size_t len, char* src);

int my_sscanf2(char* result, uint32_t result_length, char* format, va_list args);

#endif

