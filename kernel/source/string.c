#include <types.h>
#include <string.h>
#include <stdarg.h>
#include <kernel.h>
#include <terminal.h>

static int my_sscanf_core(char* result, uint32_t result_length, char* format, va_list args);

#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647

//Counts number of digits in an integer
static int int_digit_count(int n) {
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;

    if (n < 10) return 1;
    if (n < 100) return 2;
    if (n < 1000) return 3;
    if (n < 10000) return 4;
    if (n < 100000) return 5;
    if (n < 1000000) return 6;
    if (n < 10000000) return 7;
    if (n < 100000000) return 8;
    if (n < 1000000000) return 9;
    return 10;
}

char* my_strcpy(const char* src, char* dst)
{
    char *ptr = dst;
    while((*dst++ = *src++));

    return ptr;
}

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

uint32_t uatoi(const char* str)
{
    unsigned char* ustr = (unsigned char*)str;
    uint32_t res = 0;
    uint8_t c;

    while((c = *ustr++)) {
        res *= 10;
        res += (c - '0');
    }

    return res;
}

char* uitoa(uint32_t number, char* buf)
{
    buf++;
    uint32_t shifter = number;

    do {
        buf++;
        shifter /= 10;
    } while(shifter > 0);

    *--buf = '\0';

	do {
		*--buf = '0' + (number % 10); // Muahaha!
		number /= 10;
	} while(number > 0);

    return buf;
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

void dec_to_hex(char* buf, unsigned int dec, unsigned int lower_case)
{
    unsigned int reminder[50];
    unsigned int length = 0;
    char* buf_ptr = buf;

    if (dec == 0)
    {
        *buf_ptr++ = '0';
    }

    while (dec > 0)
    {
        reminder[length] = dec % 16;
        dec = dec / 16;
        length++;
    }

    if (lower_case)
        lower_case = 32;

    int i;
    for (i = length - 1; i >= 0; i--)
    {
        switch (reminder[i])
        {
        case 0:
            *buf_ptr++ = '0';
            break;
        case 10:
            *buf_ptr++ = 'A' + lower_case;
            break;
        case 11:
            *buf_ptr++ = 'B' + lower_case;
            break;
        case 12:
            *buf_ptr++ = 'C' + lower_case;
            break;
        case 13:
            *buf_ptr++ = 'D' + lower_case;
            break;
        case 14:
            *buf_ptr++ = 'E' + lower_case;
            break;
        case 15:
            *buf_ptr++ = 'F' + lower_case;
            break;
        case 16:
            *buf_ptr++ = '1';
            *buf_ptr++ = '0';
            break;
        default:
            {
                // Display as digits
                char itoa_buf[10];
                //itoa(reminder[i], itoa_buf);
                itoa(reminder[i], itoa_buf);

                my_strcpy(itoa_buf, buf_ptr);

                buf_ptr += strlen(itoa_buf);
                break;
            }
        }
    }

    *buf_ptr = '\0';
}

int my_sscanf(char* result, uint32_t result_length, char* format, ...)
{
    va_list args;
    va_start(args, format);
    int chars_read = my_sscanf_core(result, result_length, format, args);
    va_end(args);

    return chars_read;
}

int my_sscanf2(char* result, uint32_t result_length, char* format, va_list args)
{
    return my_sscanf_core(result, result_length, format, args);
}

static int my_sscanf_core(char* result, uint32_t result_length, char* format, va_list args)
{
    /* TODO: Flags
    - : Left justify (wut?)
    + : Preceed number with + or -
    # : Used with x and X to print 0x, for a,e,f,g print comma
    */

    // Keep track of length of characters we copy into result
    uint32_t return_value = 0;

    char* result_cur = result; // This moves

    uint32_t width = 0;
    bool read_width = false;
    char* cur_arg = NULL;

    do
    {
        char cur = *format;
        char next = *(format + 1);
        if (cur != '%' && read_width != 1)
        {
            return_value++;

            if (result != NULL && return_value < result_length) {
                *result_cur++ = cur;
            }
        }
        else
        {
            // If  we were reading length and reached the end of the length
            if (read_width)
            {
                next = cur;
            }

            switch (next)
            {
            case 'c': // Unsigned Character
            {
                char char_arg = (unsigned char)va_arg(args, int);

                return_value++; // for _s
                // Checks to allow for a "dry run" without data copy
                if (result != NULL && return_value < result_length) {
                    *result_cur++ = char_arg;
                }

                format++; // As this was a format specifier, make sure we skip 2

                break;
            }
            case 'd': // Signed decimal int
            {
                int arg = va_arg(args, int);

                char ds[12];

                // TODO: itoa needs to return the number of digits instead of using int_digit_count
                itoa(arg, &ds[0]);
                int arg_digit_count = int_digit_count(arg);

                // Copy it over to result
                int max_len = width > 0 ? width : arg_digit_count;

                return_value += max_len;

                if (result != NULL && return_value < result_length) {
                    for (int i = 0; i < max_len; i++)
                        *result_cur++ = ds[i];
                }

                if (width == 0)
                    format++; // As this was a format specifier, make sure we skip 2

                // Done with this width specifier
                width = 0;

                break;
            }
            case 's': // String
            {
                cur_arg = (char*)va_arg(args, int);

                // Invalid string, cancel out before we attempt to read a null pointer
                if (cur_arg == NULL)
                    return return_value;

                int arg_chars_read = 0;
                do
                {
                    return_value++;

                    if (result != NULL && return_value < result_length) {
                        *result_cur++ = *cur_arg++;
                    }

                    arg_chars_read++;
                } while (*cur_arg != 0 && (width == 0 || arg_chars_read < width));

                if (width == 0)
                    format++;

                // Done with this width specifier
                width = 0;

                break;
            }
            case 'x': // unsigned hexadecimal int
            case 'X': // unsigned hexadecimal int (uppercase)
            case 'p': // Pointer - Write hex address
            case 'P': // Pointer - Write hex address (uppercase)
            {
                unsigned int lower_case = next == 'x' || next == 'p';

                int hexArg = va_arg(args, int);

                char hexStr[9];
                dec_to_hex(hexStr, hexArg, lower_case);

                if (width > 0)
                {
                    int zeroes_to_pad = width - strlen(hexStr);

                    // Hex string limit of 8 characters (32-bit)
                    if (zeroes_to_pad > 8 - strlen(hexStr))
                        zeroes_to_pad = 8 - strlen(hexStr);

                    if (zeroes_to_pad > 0)
                    {
                        // Shift the entire string over
                        unsigned int i, j;
                        j = 0;
                        for (i = 8 - zeroes_to_pad; i > 0; i--)
                        {
                            // Fix this nasty shit
                            hexStr[zeroes_to_pad + i - 1] = hexStr[8 - zeroes_to_pad - j - 1];
                            j++;
                        }

                        // Insert zeroes
                        for (i = 0; i < zeroes_to_pad; i++)
                            hexStr[i] = '0';

                        hexStr[8] = 0; // Null terminated
                    }
                }

                // Pointer - Prefix with 0x
                if (next == 'p' || next == 'P')
                {
                    return_value += 2;
                    if (result != NULL && return_value < result_length) {
                        *result_cur++ = '0';
                        *result_cur++ = 'x';
                    }
                }

                return_value += strlen(hexStr);

                if (result != NULL && return_value < result_length) {
                    my_strcpy(&hexStr[0], result_cur);
                    result_cur += strlen(hexStr);
                }

                // Format specifier, skip 2
                if (width == 0)
                    format++;

                // Done with this width specifier
                width = 0;
                read_width = false;

                break;
            }
            case 'f': // TODO: Decimal float
                break;
            case 'a': // TODO: Hexadecimal float
                break;
            case 'A': // TODO: Hexadecimal float (uppercase)
                break;
            case '%': // Escaped %
                // (Just copy both across directly)
                return_value += 1;
                if (result != NULL && return_value < result_length) {
                    // Only copy one, it's escaped!
                    *result_cur++ = cur;
                }
                format += 1;
                break;
            case '*': // Width specified in arg
            {
                width = va_arg(args, int);

                format++;
                read_width = true;

                continue;
            }
            default:
                // Length specifier?
                if (next >= '0' && next <= '9')
                {
                    format++; // Skip past the % and read the length
                    do
                    {
                        width = (width * 10) + (next - '0');

                        format++;
                        next = *format;
                    } while (next >= '0' && next <= '9');

                    read_width = true;

                    // Rewind so we can read the format type
                    format--;

                    continue;
                }

                break;
            }
        }

        // We've handled one arg after reading the length specifier, reset flag
        read_width = 0;
    } while (*++format != 0);

    // Null-terminate% the string
    if (result != NULL && return_value < result_length) {
        *result_cur = 0;
    }

    return return_value;
}



