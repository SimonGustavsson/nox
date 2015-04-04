#ifndef NOX_KERNEL_H
#define NOX_KERNEL_H

#define PACKED __attribute__((__packed__))
#define SECTION_BOOT __attribute__((section(".text.boot")))
#define NO_INLINE __attribute__((noinline))

enum kresult
{
    kresult_ok = 0,
    kresult_invalid = -1
};

#endif
