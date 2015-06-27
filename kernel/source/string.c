#include <types.h>
#include <string.h>

size_t strlen(const char* str)
{
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}

char* kstrcpy_n(char* dest, size_t len, char* src)
{
    for(size_t i = 0; i < len; i++) {
        *dest++ = *src++;
    }

    return dest;
}

bool kstrcmp(const char* a, const char* b)
{
    size_t a_len = strlen(a);
    size_t b_len = strlen(b);

    if(a_len != b_len) {
        return false;
    }

    return kstrcmp_n(a, b, a_len);
}

bool kstrcmp_n(const char* a, const char* b, size_t len)
{
    for(size_t i = 0; i < len; i++) {
        if(a[i] != b[i])
            return false;
    }

    return true;
}

char* itoa(int32_t number, char* buf) {

	// We populate the string backwards, increment to make room for \0
	buf++;

	bool negative = number < 0;
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

    return buf;
}

uint8_t nybble_to_ascii(uint8_t val)
{
	if (val < 0x0A)
		return '0' + val;
	else
		return 'A' + (val - 10);
}

