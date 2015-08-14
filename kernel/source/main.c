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
#include <mem_mgr.h>
#include <ata.h>
#include <fs.h>
#include <fat.h>
#include <elf.h>
#include <pic.h>
#include <paging.h>

// LOL
#include <arch/cpu.h>

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
    KWARN("Welcome to the glorious wunderkind of Philip & Simon!");
}

SECTION_BOOT void _start(struct mem_map_entry mem_map[], uint32_t mem_entry_count)
{
    stack_set(0x1337000);

    screen_init();
    screen_cursor_hide();

    terminal_init();

    print_welcome();

    interrupt_init_system();
    interrupt_receive_trap(0x80, isr_syscall);

    pic_init();

    mem_mgr_init(mem_map, mem_entry_count);
    mem_mgr_gdt_setup();

    // Just print some page table stuff to verify that it works
    uintptr_t pd = page_directory_create();
    //page_allocate(pd, 0x1337000 - 4096, 0x1337000 - 4096, (1 << 1) | (1 << 3));
    print_pde(pd, 1);
    uintptr_t pt0 = (uintptr_t) (*(uint32_t*)pd & 0xFFFFF000);
    print_pte(pt0, 0);

    SHOWVAL_x("The page directory is at: ", (uint32_t)pd);
    page_directory_install(pd);

    kb_init();

    // Let's do some hdd stuff m8
    ata_init();

    fs_init();

    pit_init();

    usb_init();

    call_test_sys_call(0x1234);

    terminal_write_string("Kernel initialized, off to you, interrupts!\n");

    //elf_run("USERLANDELF");

    cli_init();

    // Re-enable interrupts, we're ready now!
    interrupt_enable_all();

    cli_run();

    while(1);
}

