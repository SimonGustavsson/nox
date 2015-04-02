#ifndef NOX_DEBUG_H
#define NOX_DEBUG_H

#define DEBUG(STR, PARAMS...) printf(STR "\n", ##PARAMS);

#ifdef TARGET_GDB
#define BREAK() __asm ("int $3")
#elif  TARGET_BOCHS
#define BREAK() __asm ("xchg %bx, %bx")
#else
#define BREAK()
#endif

#endif

