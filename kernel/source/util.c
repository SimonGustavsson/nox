#include <util.h>

void* my_memcpy(const void *dest, const void *src, unsigned int bytesToCopy)
{
    char *s = (char *)src;
    char *d = (char *)dest;
    while (bytesToCopy > 0)
    {
        *d++ = *s++;
        bytesToCopy--;
    }
    return (void*)dest; // Disregards const modifier
}

void* my_memset(void* dest, unsigned char c, unsigned int size)
{
    unsigned char* ptr = (unsigned char*)dest;

    while (size--)
        *ptr++ = c;

    return dest;
}

