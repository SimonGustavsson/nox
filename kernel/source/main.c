#include <types.h>
#include <screen.h>
#include <kernel.h>
#include <debug.h>
#include <terminal.h>
#include <pci.h>
#include <pic.h>
#include <interrupt.h>
#include <pit.h>
#include <pio.h>
#include <keyboard.h>
#include <uhci.h>
#include <usb.h>
#include <cli.h>

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
    terminal_write_uint32_x(regs->eax);
    terminal_write_string("\n");
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

enum region_type {
    region_type_normal = 0x01,
    region_type_reserved = 0x02,
    region_type_acpi_reclaimable = 0x03,
    region_type_acpi_nvs = 0x04,
    region_type_bad = 0x05
};

enum acpi_attr_flags {
    acpi_attr_flags_do_not_ignore = 0x01,
    acpi_attr_flags_non_volatile = 0x02
};

struct PACKED mem_map_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attr;
};

SECTION_BOOT void _start(struct mem_map_entry mem_map[], uint32_t mem_entry_count)
{
    screen_init();
    screen_cursor_hide();

    terminal_init();

    print_welcome();

    interrupt_init_system();
    interrupt_receive_trap(0x0D, gpf);
    interrupt_receive_trap(0x80, isr_syscall);

    // Remap the interrupts fired by the PICs
    pic_init();

    kb_init();

    KWARN("Nox has colored output, lets use it!");

    terminal_write_string("We have ");
    terminal_write_uint32(mem_entry_count);
    terminal_write_string(" entries at ");
    terminal_write_ptr(mem_map);
    terminal_write_string("\n");

    for (int i = 0; i < mem_entry_count; i++) {
        terminal_write_string("Entry ");
        terminal_write_uint32_x(i);
        terminal_write_string(": ");
        terminal_write_uint64_x(mem_map[i].base);
        terminal_write_string(" (");
        terminal_write_uint64_x(mem_map[i].length);
        terminal_write_string(") Type: ");
        terminal_write_uint32_x(mem_map[i].type);
        terminal_write_string("\n");
    }

    // Re-enable interrupts, we're ready now!
    interrupt_enable_all();

    call_test_sys_call(0x1234);

    pit_set(1000);

    usb_init();

    terminal_write_string("Kernel initialized, off to you, interrupts!\n");

    cli_init();
    cli_run();

    while(1);
}

