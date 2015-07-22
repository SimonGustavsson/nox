#ifndef NOX_MEM_MGR_H
#define NOX_MEM_MGR_H

#define PAGE_SIZE (4096)

#define GDT_SELECTOR(Index, PrivLevel) ((uint16_t)((Index << 3) + (0 << 2) + PrivLevel))
#define LDT_SELECTOR(Index, PrivLevel) ((uint16_t)((Index << 3) + (1 << 2) + PrivLevel))

#define KERNEL_CODE_SEGMENT GDT_SELECTOR(1, 0)
#define KERNEL_DATA_SEGMENT GDT_SELECTOR(2, 0)
#define USER_CODE_SEGMENT   GDT_SELECTOR(3, 3)
#define USER_DATA_SEGMENT   GDT_SELECTOR(4, 3)

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
void mem_mgr_gdt_setup();

size_t mem_page_count(bool get_allocated);
bool mem_page_reserve(const char* identifier, void* address, size_t num_pages);
void*  mem_page_get();
void*  mem_page_get_many(uint16_t how_many);
void   mem_page_free(void* address);
void   mem_print_usage();

#endif
