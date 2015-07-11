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
#include <elf.h>
#include <pic.h>

static void gpf(uint8_t irq, struct irq_regs* regs)
{
    BREAK();
    KERROR("FAULT: Generation Protection Fault!");
    //uint32_t* saved_esp = (uint32_t*)(intptr_t)(regs->esp);

    //SHOWVAL_x("Saved ESP Addr: ", (uint32_t)saved_esp);

    //for(size_t i = 0; i < 10; i++) {
    //    terminal_write_string("ESP+");
    //    terminal_write_uint32(i);
    //    terminal_write_string(": ");
    //    terminal_write_uint32_x(*(saved_esp + i));
    //    terminal_write_char('\n');
    //}
    //BREAK();
}

static void page_fault(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Page fault!");
    BREAK();
}

static void stack_segment_fault(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Stack segment fault!");
    BREAK();
}

static void invalid_tss(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Invalid-TSS!");
    BREAK();
}

static void segment_not_present(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Segment not present");
    BREAK();
}

static void double_fault(uint8_t irq, struct irq_regs* regs)
{
    KERROR("Double fault!");
    BREAK();
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
    KWARN("Welcome to the glorious wunderkind of Philip & Simon!");
}

SECTION_BOOT void _start(struct mem_map_entry mem_map[], uint32_t mem_entry_count)
{
    screen_init();
    screen_cursor_hide();

    terminal_init();

    print_welcome();

    interrupt_init_system();
    interrupt_receive_trap(0x08, double_fault);
    interrupt_receive_trap(0x0A, invalid_tss);
    interrupt_receive_trap(0x0B, segment_not_present);
    interrupt_receive_trap(0x0C, stack_segment_fault);
    interrupt_receive_trap(0x0D, gpf);
    interrupt_receive_trap(0x0E, page_fault);
    interrupt_receive_trap(0x80, isr_syscall);

    pic_init();

    mem_mgr_init(mem_map, mem_entry_count);
    mem_mgr_gdt_setup();

    kb_init();

    // Let's do some hdd stuff m8
    ata_init();

    fs_init();

    pit_set(1000);

    usb_init();

    call_test_sys_call(0x1234);

    terminal_write_string("Kernel initialized, off to you, interrupts!\n");

    //elf_run("USERLANDELF");

    // Re-enable interrupts, we're ready now!
    interrupt_enable_all();

    cli_init();
    cli_run();

    while(1);
}

