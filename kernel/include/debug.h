#ifndef NOX_DEBUG_H
#define NOX_DEBUG_H

#define DEBUG(STR, PARAMS...) printf(STR "\n", ##PARAMS);

#if PLATFORM_DEBUG_GDB
    #define BREAK() __asm ("int $3")
#elif  PLATFORM_DEBUG_BOCHS
    #define BREAK() __asm ("xchg %bx, %bx")
#else
    #warning Unknown Debugging Platform
    #define BREAK()
#endif

#endif

