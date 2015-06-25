#include <types.h>
#include <fs.h>

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

