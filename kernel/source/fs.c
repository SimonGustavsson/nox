#include <types.h>
#include <kernel.h>
#include <fs.h>
#include <mem_mgr.h>
#include <ata.h>
#include <fat.h>
#include <terminal.h>
struct fat_part_info g_fat;

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

        if(!fat_init(&mbr->partitions[i], &g_fat)) {
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
    return &g_fat;
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

