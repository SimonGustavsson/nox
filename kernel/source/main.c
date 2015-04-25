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
    // We deal with all input in key up! :-)
}

void key_up(enum keys key)
{
    char c = kb_key_to_ascii(key);
    if(c < 0) {
    
        // Perhaps it's a key such as ctrl?
        char* key_name = kb_get_special_key_name(key);

        terminal_write_char('~');
        if(key_name != NULL) {
            terminal_write_string(key_name);
        }
        else {
            terminal_write_string("Unprintable key released. keys_ value: ");
            terminal_write_hex((uint32_t)key);
            terminal_write_string("\n");
        }
    }
    else {
        terminal_write_char(c);
    }
}

void print_welcome()
{
    terminal_write_string("| \\ | |/ __ \\ \\ / /  \n");
    terminal_write_string("|  \\| | |  | \\ V /   \n");
    terminal_write_string("| . ` | |  | |> <    \n");
    terminal_write_string("| |\\  | |__| / . \\   \n");
    terminal_write_string("|_| \\_|\\____/_/ \\_\\  \n");
    terminal_write_string("Welcome to the glorious wunderkind of Philip & Simon!\n");
}

SECTION_BOOT void _start()
{
    terminal_init();

    print_welcome();

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

    KWARN("Nox has colored output, lets use it!");

    terminal_write_string("Kernel initialized, off to you, interrupts!\n");

    while(1);
}

