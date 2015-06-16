#ifndef NOX_MEM_MGR_H
#define NOX_MEM_MGR_H

enum region_type {
    region_type_normal = 0x01,
    region_type_reserved = 0x02,
    region_type_acpi_reclaimable = 0x03,
    region_type_acpi_nvs = 0x04,
    region_type_bad = 0x05
};

enum acpi_attr_flags {
    acpi_attr_flags_do_not_ignore = 0x01,
    acpi_attr_flags_non_volatile = 0x02
};

struct PACKED mem_map_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attr;
};

void mem_mgr_init(struct mem_map_entry mem_map[], uint32_t mem_entry_count);

#endif
