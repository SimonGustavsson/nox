#include <types.h>

void cpu_reset()
{
    __asm("mov $0xFE, %%al;"
          "out %%al, $0x64;"
          :
          :
          :
          );
}

void stack_set(uintptr_t stack_location)
{
    // We're about to overwrite the stack pointer,
    // when this function exits, three things are going to happen.
    //
    // First, we're going to pop ebp, which the function prologue
    // pushed at the very start of the function. So we need to
    // ensure the new stack has the original ebp value because
    // the caller may depend on it.
    //
    // Second, we're going to do a ret, which is going to POP the
    // return address from the new stack, so we need to ensure the head
    // of the stack is the return address we were passed.
    //
    // Third, the caller is going to pop the argument it pushed
    // on to the stack before calling us, so we need to make
    // sure that there's something there for it to pop (we just
    // push a scalar value of the same size as it gave us)

    __asm("pop %%eax;           "   // Store EBP
          "pop %%ebx;           "   // Store Return Address
          "mov %0, %%esp;       "   // Update ESP
          "push $0;             "   // Push dummy argument
          "push %%ebx;          "   // Push return address
          "push %%eax;          "   // Push original EBP
            :
            : "r"(stack_location)
            : "eax"
         );
}
