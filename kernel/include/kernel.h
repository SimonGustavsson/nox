#ifndef NOX_KERNEL_H
#define NOX_KERNEL_H

#define PACKED __attribute__((__packed__))
#define SECTION_BOOT __attribute__((section(".text.boot")))

#if !defined(__cplusplus)
#include <stdbool.h> // C Does not have bool :(
#endif

#include <stdint.h>

#endif
