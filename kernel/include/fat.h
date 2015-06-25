#ifndef NOX_FAT_H
#define NOX_FAT_H

struct ebpb_fat16 {
    uint8_t      drive_number;
    uint8_t      reserved1;
    uint8_t      boot_signature;
    uint32_t     volume_id;
    uint8_t      volume_label[11];
    uint8_t      file_sys_Type[8];
};

struct ebpb_fat32 {
    uint32_t     fat_size32;
    uint16_t     ext_flags;
    uint16_t     fs_version;
    uint32_t     root_cluster;
    uint16_t     fs_info;
    uint16_t     backup_boot_sector;
    uint8_t      reserved[12];
    uint8_t      drive_number;
    uint8_t      reserved1;
    uint8_t      boot_signature;
    uint32_t     volume_id;
    uint8_t      volume_label[11];
    uint8_t      file_sys_type[8];
};

struct bpb { 
    uint8_t      jmp_boot[3];
    uint8_t      oem_name[8];
	uint16_t     bytes_per_sector;
	uint8_t      sectors_per_cluster;
	uint16_t     reserved_sector_count;
	uint8_t      num_fats;
	uint16_t     root_entry_count;
	uint16_t     total_sectors16;
	uint8_t      media;
	uint16_t     fat_size16;
	uint16_t     sectors_per_track;
	uint16_t     num_heads;
	uint32_t     hidden_sectors;
	uint32_t     total_sectors32; // use if TotalSectors16 = 0
	union {
		struct ebpb_fat16 ebpb;
		struct ebpb_fat32 ebpb32;
	} ebpb;
} bpb;

enum fat_version {
    fat_version_12,
    fat_version_16,
    fat_version_32
};

struct fat_dir_entry {
	uint8_t     name[11];
	uint8_t     attribute;
	uint8_t     create_time[3];
	uint16_t    create_date;
	uint16_t    last_access_date;
	uint16_t    last_access_time;
	uint16_t    last_modified_time;
	uint16_t    last_modified_date;
	uint16_t    first_cluster;
	uint32_t    size;
};

struct fat_part_info {
    struct mbr_partition_entry mbr_entry;
    uint32_t                  root_dir_sector;
    uint32_t                  num_root_dir_sectors;
    uint32_t                  data_begin;
    uint32_t                  num_sectors_per_cluster;
    uint32_t                  fat_begin;
    uint32_t                  fat_size;
    uint32_t                  total_sectors;
};

enum fat_version fat_get_version(struct bpb* bpb);
bool fat_init(struct mbr_partition_entry* partition_entry, struct fat_part_info* info_result);

#endif

