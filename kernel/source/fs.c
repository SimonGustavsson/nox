#include <types.h>
#include <kernel.h>
#include <fs.h>
#include <mem_mgr.h>
#include <ata.h>
#include <fat.h>
#include <terminal.h>

struct fat_part_info g_system_part;

bool fs_init()
{
    // Initialize file system
    uint32_t* buffer = (uint32_t*)(mem_page_get());

    if(!ata_read_sectors(0, 1, (intptr_t)buffer)) {
        KERROR("Failed to read MBR!");
        return false;
    }

    struct mbr* mbr = (struct mbr*)(buffer);
    for(int i = 0; i < 4; i++) {
        if(!fs_is_fat_type(mbr->partitions[i].type))
            continue;

        if(!fat_init(&mbr->partitions[i], &g_system_part)) {
            KWARN("Failed to initialize FAT file system!");
            continue;
        }

        // We're only supposed to have one partition
        KINFO("System partition initialized");
        return true;
    }

    KERROR("No FAT partitions found!");
    return false;
}

struct fat_part_info* fs_get_system_part()
{
    return &g_system_part;
}

const char* fs_get_printable_partition_type(enum partition_type type)
{
    switch(type) {
        case partition_type_unknown:     return "Unknown";
        case partition_type_fat12:       return "FAT12";
        case partition_type_fat16_small: return "FAT16 (<32MB)";
        case partition_type_ext_chs:     return "Extended (CHS)";
        case partition_type_fat16_large: return "FAT16 (>32MB)";
        case partition_type_fat32_chs:   return "FAT32 (CHS)";
        case partition_type_fat32_lba:   return "FAT32 (LBA)";
        case partition_type_fat16b_lba:  return "FAT16B (LBA)";
        case partition_type_ext_lba:     return "EXT (LBA)";
        default:                         return "Unrecognized";
    }
}

bool fs_is_fat_type(enum partition_type type)
{
    return
        type == partition_type_fat16_small ||
        type == partition_type_fat16_large ||
        type == partition_type_fat32_chs   ||
        type == partition_type_fat32_lba   ||
        type == partition_type_fat16b_lba;
}

void fs_cat(const char* filename)
{
    struct fat_dir_entry entry;
    if(!fat_get_dir_entry(&g_system_part, filename, &entry)) {
        terminal_write_string("No such file '");
        terminal_write_string(filename);
        terminal_write_string("'\n");
        return;
    }

    size_t pages_req = entry.size / PAGE_SIZE;
    if(entry.size % PAGE_SIZE)
        pages_req++;

    intptr_t buffer = (intptr_t)mem_page_get_many(pages_req);

    if(!fat_read_file(&g_system_part, &entry, buffer, pages_req * PAGE_SIZE)) {
        KERROR("Failed to read file");
        return;
    }

    if(entry.size > 100) {
        terminal_write_string_n((char*)buffer, 100);
        terminal_write_string("...\n");
    }
    else {
        terminal_write_string_n((char*)buffer, entry.size);
        terminal_write_char('\n');
    }
}

