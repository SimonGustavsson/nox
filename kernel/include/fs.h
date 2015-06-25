#ifndef NOX_FS_H
#define NOX_FS_H

enum partition_type {
    partition_type_unknown          = 0x00,
    partition_type_fat12            = 0x01,
    partition_type_fat16_small      = 0x04,
    partition_type_ext_chs          = 0x05,
    partition_type_fat16_large      = 0x06,
    partition_type_fat32_chs        = 0x0B, // Partitions up to 2047MB
    partition_type_fat32_lba        = 0x0C, // Lba extension
    partition_type_fat16b_lba       = 0x0E,
    partition_type_ext_lba          = 0xF,
};

struct mbr_partition_entry {
    uint8_t status;
    uint8_t chs_begin[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_begin;
    uint32_t num_sectors;
} PACKED;

struct mbr {
    uint8_t                     code[446];
    struct mbr_partition_entry  partitions[4];
    uint16_t                    signature;
};

const char* fs_get_printable_partition_type(enum partition_type type);
bool fs_is_fat_type(enum partition_type type);

#endif

