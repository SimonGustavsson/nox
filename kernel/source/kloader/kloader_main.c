#include <kernel.h>
#include <types.h>
#include <mem_mgr.h>
#include <screen.h>
#include <terminal.h>
#include <ata.h>
#include <fs.h>
#include <fat.h>
#include <debug.h>
#include <elf.h>

#define KERNEL_LOAD_ADDRESS 0x100000

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
    if(!fat_get_dir_entry(part_info, "KERNEL  ELF", &kernel)) {
        KPANIC("Failed to locate KERNEL.ELF");
        while(1);
    }

    intptr_t kernel_entry_point;

    if(!elf_load_trusted("KERNEL  ELF", &kernel_entry_point)) {
        KWARN("Failed to load elf!");
    } 

    kernel_entry cmain = (kernel_entry)(kernel_entry_point);

    cmain(mem_map, mem_entry_count);


    KINFO("Bootloader done");

    while(1);
}


