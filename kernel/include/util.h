// util.h
#ifndef UTIL_H
#define UTIL_H

#define assert(expression) (!(expression) ? printf("Assertion failed at %s(%d), expression: " #expression, __FILE__, __LINE__) : (void)0)
#define assert2(expression, message) (!(expression) ? printf("Assertion failed at %s(%d), expression: " #expression ": %s", __FILE__, __LINE__, message) : (void)0)
#define assert3(expression, message, ...) (!(expression) ? printf(message, __VA_ARGS__) : (void)0)

void* my_memcpy(const void *dest, const void *src, unsigned int bytesToCopy);
void* my_memset(void* dest, unsigned char c, unsigned int size);

#endif

