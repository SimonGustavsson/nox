#include <types.h>
#include "string.h"

size_t strlen(const char* str)
{
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}

bool kstrcmp(char* a, char* b)
{
    size_t a_len = strlen(a);
    size_t b_len = strlen(b);

    if(a_len != b_len) {
        return false;
    }

    for(size_t i = 0; i < a_len; i++) {
        if(a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

void itoa(int number, char* buf)
{
	// We populate the string backwards, increment to make room for \0
	buf++;

	int negative = number < 0;
	if(negative)
	{
		buf++;
		number = -number;
	}

	// Find where our string will end
	int shifter = number;
	do
	{
		buf++;
		shifter /= 10;
	}while(shifter > 0);

	// Make sure the string is terminated nicely
	*--buf = '\0';

	// Start converting the digits into characters
	do
	{
		*--buf = '0' + (number % 10); // Muahaha!
		number /= 10;
	}while(number > 0);

	if(negative)
		*--buf = '-';
}

uint8_t nybble_to_ascii(uint8_t val)
{
	if (val < 0x0A)
		return '0' + val;
	else
		return 'A' + (val - 10);
}

