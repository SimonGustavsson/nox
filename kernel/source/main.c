#include <kernel.h>
#include <debug.h>
#include <types.h>
#include <terminal.h>
#include <pci.h>
#include <pic.h>
#include <interrupt.h>
#include <pit.h>
#include <pio.h>
#include <keyboard.h>

const char* gHcVersionNames[4] = {"UHCI", "OHCI", "EHCI", "xHCI"};

static void gpf(uint8_t irq, struct irq_regs* regs)
{
    terminal_write_string("General Protection Fault!\n");
}

static void call_test_sys_call(uint32_t foo)
{
    __asm("mov %0, %%eax"
            :
            : "r"(foo)
            : "eax");
    __asm("int $0x80");
}

void isr_syscall(uint8_t irq, struct irq_regs* regs)
{
    terminal_write_string("0x80 Sys Call, foo: ");
    terminal_write_hex(regs->eax);
    terminal_write_string("\n");
}

void key_down(enum keys key)
{
    char c = kb_key_to_ascii(key);

    terminal_write_string("\n");
    terminal_write_char(c);
    terminal_write_string(" was pressed!");
}

void key_up(enum keys key)
{
    char c = kb_key_to_ascii(key);
    terminal_write_string("\n");
    terminal_write_char(c);
    terminal_write_string(" was released!");
}

SECTION_BOOT void _start()
{
    terminal_init();
    terminal_write_string("NOX is here, bow down puny mortal...\n");

    interrupt_init_system();
    interrupt_receive_trap(0x0D, gpf);
    interrupt_receive_trap(0x80, isr_syscall);

    // Remap the interrupts fired by the PICs
    pic_init();

    kb_init();

    struct kb_subscriber kb = {
        .down = key_down,
        .up = key_up
    };

    kb_subscribe(&kb);

    // Re-enable interrupts, we're ready now!
    interrupt_enable_all();

    call_test_sys_call(0x1234);

    pit_set(1000);

    terminal_write_string("\nKernel done, halting!\n");
    KWARN("Nox has colored output, lets use it!");
    while(1);
}
