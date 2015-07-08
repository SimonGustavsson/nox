#include <kernel.h>
#include <types.h>
#include <mem_mgr.h>
#include <screen.h>
#include <terminal.h>
#include <ata.h>
#include <fs.h>
#include <fat.h>
#include <debug.h>

typedef void ((*kernel_entry)(struct mem_map_entry[], uint32_t));

void kloader_cmain(struct mem_map_entry mem_map[], uint32_t mem_entry_count)
{
    screen_init();
    screen_cursor_hide();
    terminal_init();

    KINFO("Welcome to Nox (Bootloader mode)");

    mem_mgr_init(mem_map, mem_entry_count);

    ata_init();
    fs_init();

    struct fat_part_info* part_info = fs_get_system_part();
    struct fat_dir_entry kernel;
    if(!fat_get_dir_entry(part_info, "KERNEL  BIN", &kernel)) {
        KPANIC("Failed to locate KERNEL.BIN");
        while(1);
    }

    size_t pages_req = kernel.size / PAGE_SIZE;
    if(kernel.size % PAGE_SIZE)
        pages_req++;

    uint32_t load_addr = 0x10000;
    intptr_t buffer = (intptr_t)load_addr;

    if(!fat_read_file(part_info, &kernel, buffer, pages_req * PAGE_SIZE)) {
        KPANIC("Failed to read KERNEL.BIN");
    }

    kernel_entry cmain = (kernel_entry)(buffer);

    cmain(mem_map, mem_entry_count);

    while(1);
}


