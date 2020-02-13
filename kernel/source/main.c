#include <types.h>
#include <screen.h>
#include <memory.h>
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
#include <multiboot2.h>

#define MAX_MMAP_ENTRIES 10
static struct mem_map_entry g_mmap[MAX_MMAP_ENTRIES];
static uint32_t g_mmap_entries;

extern uint32_t hello_rust(uint32_t, uint32_t);

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

enum region_type mem_type_from_multiboot(uint32_t multiboot_mem_type)
{
    switch (multiboot_mem_type) {
        case MULTIBOOT_MEMORY_AVAILABLE:
            return region_type_normal;
        case MULTIBOOT_MEMORY_RESERVED:
            return region_type_reserved;
        case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
            return region_type_acpi_reclaimable;
        case MULTIBOOT_MEMORY_NVS:
            return region_type_acpi_nvs;
        case MULTIBOOT_MEMORY_BADRAM:
            return region_type_bad;
        default:
            return region_type_reserved;
    }
}

void init_memory_map(uint32_t addr)
{
    struct multiboot_tag* tag;
    for (tag = (struct multiboot_tag*) (addr + 8);
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag *) ((uint8_t *) tag
                + ((tag->size + 7) & ~7)))
    {
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_MMAP:
                {
                    struct multiboot_tag_mmap* mmap = (struct multiboot_tag_mmap*) tag;
                    // -16 to skip type/size/entry_size/entry_version
                    uint32_t num_entries = (mmap->size - 16) / mmap->entry_size;

                    if (num_entries > MAX_MMAP_ENTRIES) {
                        KWARN("WARNING. SOME MMAP ENTRIES WILL BE DROPPED!\n");
                    }

                    for (int i = 0; i <= num_entries; i++) {
                        struct multiboot_mmap_entry* cur = &mmap->entries[i];

                        g_mmap_entries++;

                        g_mmap[i].base = cur->addr;
                        g_mmap[i].length = cur->len;
                        g_mmap[i].type = (uint32_t) mem_type_from_multiboot(cur->type);
                    }
                }
                break;
        }
    }
}

void read_tags2(uint32_t addr)
{
    struct multiboot_tag* tag;
    for (tag = (struct multiboot_tag*) (addr + 8);
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag *) ((uint8_t *) tag
                + ((tag->size + 7) & ~7)))
    {
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                {
                    struct multiboot_tag_string* cmd = (struct multiboot_tag_string*) tag;

                    terminal_write_string("Cmd line: ");
                    terminal_write_string(cmd->string);
                    terminal_write_char('\n');
                }
                break;
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                {
                    struct multiboot_tag_basic_meminfo* basic = (struct multiboot_tag_basic_meminfo*) tag;
                    terminal_write_string("Basic mem, low: ");
                    terminal_write_uint32_x(basic->mem_lower);
                    terminal_write_string(" upper: ");
                    terminal_write_uint32_x(basic->mem_upper);
                    terminal_write_char('\n');
                }
                break;
            case MULTIBOOT_TAG_TYPE_BOOTDEV:
                break;
            case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
                break;
            case MULTIBOOT_TAG_TYPE_APM:
                break;
            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
                break;
          case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                {
                    struct multiboot_tag_framebuffer* fb = (struct multiboot_tag_framebuffer*) tag;
                    terminal_write_string("Framebuffer @ ");
                    terminal_write_uint64_x(fb->common.framebuffer_addr);
                    terminal_write_char(' ');
                    terminal_write_uint32(fb->common.framebuffer_width);
                    terminal_write_char('x');
                    terminal_write_uint32(fb->common.framebuffer_height);
                    terminal_write_string(" (");
                    terminal_write_uint32(fb->common.framebuffer_bpp);
                    terminal_write_string(" bpp)\n");
                }
              break;
          case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
              {
                    struct multiboot_tag_string* bl_name = (struct multiboot_tag_string*) tag;
                    terminal_write_string("Boot loader name: ");
                    terminal_write_string(bl_name->string);
                    terminal_write_char('\n');
              }
              break;
          case MULTIBOOT_TAG_TYPE_MMAP:
              {
                  struct multiboot_tag_mmap* mmap = (struct multiboot_tag_mmap*) tag;
                  // -16 to skip type/size/entry_size/entry_version
                  uint32_t num_entries = (mmap->size - 16) / mmap->entry_size;
                  for (int i = 0; i <= num_entries; i++) {
                      struct multiboot_mmap_entry* cur = &mmap->entries[i];
                      terminal_write_string("MEM: ");
                      terminal_write_uint64_x(cur->addr);
                      terminal_write_string(", size: ");
                      terminal_write_uint64_x(cur->len);
                      terminal_write_char(' ');

                      switch (cur->type) {
                          case MULTIBOOT_MEMORY_AVAILABLE:
                              terminal_write_string("Available\n");
                              break;
                          case MULTIBOOT_MEMORY_RESERVED:
                              terminal_write_string("Reserved\n");
                              break;
                          case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
                              terminal_write_string("Reclaimable\n");
                              break;
                          case MULTIBOOT_MEMORY_NVS:
                              terminal_write_string("Nvs\n");
                              break;
                          case MULTIBOOT_MEMORY_BADRAM:
                              terminal_write_string("BAD\n");
                              break;
                          default:
                              terminal_write_string("Unknown\n");
                      }
                  }
              }
              break;
          case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR:
              {
              struct multiboot_tag_load_base_addr* base_addr =
                  (struct multiboot_tag_load_base_addr*) tag;


              terminal_write_string("Base load address: ");
              terminal_write_uint32_x(base_addr->load_base_addr);
              terminal_write_char('\n');
              }
              break;
          case MULTIBOOT_TAG_TYPE_END:
              terminal_write_string("Found end tag\n");
              return;
          default:
              terminal_write_string("Found unknown tag: ");
              terminal_write_uint32(tag->type);
              terminal_write_char('\n');
              break;
      }
  }
}

void cmain(uint32_t magic_value, uint32_t* boot_data)
{
    screen_init();
    screen_cursor_hide();

    terminal_init();

    print_welcome();

    terminal_write_string("Magic bootlaoder bytes: ");
    terminal_write_uint32_x(magic_value);
    terminal_write_string(" data sz: ");
    terminal_write_uint32_x(*(boot_data));
    terminal_write_char('\n');

    read_tags2((uint32_t) boot_data);
    init_memory_map((uint32_t) boot_data);

    terminal_write_string("Found ");
    terminal_write_uint32(g_mmap_entries);
    terminal_write_string(" mmap entries from multiboot\n");

    interrupt_init_system();
    interrupt_receive_trap(0x80, isr_syscall);

    pic_init();

    mem_mgr_init(&g_mmap[0], g_mmap_entries);

    terminal_write_string("mem_mgr_init done\n");

    memory_init();
    terminal_write_string("memory_init done\n");

    kb_init();
    terminal_write_string("kb_init done\n");

    KERROR("MOO!?");

    uint32_t rust_result = hello_rust(1, 2);
    terminal_write_string("Rust reuslt: ");
    terminal_write_uint32_x(rust_result);
    terminal_write_char('\n');

    // Let's do some hdd stuff m8
    ata_init();
    terminal_write_string("ata_init done\n");

    fs_init();
    terminal_write_string("fs_init done\n");

    pit_init();
    terminal_write_string("pit_init done\n");

    // Re-enable interrupts, we're ready now!
    interrupt_enable_all();

    KWARN("Interrupts are now enabled!");

    usb_init();

    call_test_sys_call(0x1234);

    terminal_write_string("Kernel initialized, off to you, interrupts!\n");

    //elf_run("USERLANDELF");

    cli_init();

    cli_run();

    while(1);
}

