#include <kernel.h>
#include <types.h>
#include <mem_mgr.h>
#include <terminal.h>

// -------------------------------------------------------------------------
// Global variables
// -------------------------------------------------------------------------
extern uint32_t LD_KERNEL_START;
extern uint32_t LD_KERNEL_DATA_END;
extern uint32_t LD_KERNEL_END;

struct mem_map_entry** g_mem_map;
uint32_t g_mem_map_entries;

// -------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------
static char* get_mem_type(enum region_type type);
static void print_available_memory(uint64_t available_memory);
static void print_mem_entry(size_t index, uint64_t base, uint64_t length, char* description);

// -------------------------------------------------------------------------
// Public Contract
// -------------------------------------------------------------------------
void mem_mgr_init(struct mem_map_entry mem_map[], uint32_t mem_entry_count)
{
    g_mem_map = &mem_map;
    g_mem_map_entries = mem_entry_count;

    if(mem_entry_count == 0) {
        terminal_write_string("Surprisignly enough, we have all of memory to ourselves!\n");
        return;
    }

    terminal_write_string("System memory map\n");

    uint64_t available_memory = 0;
    for (int i = 0; i < mem_entry_count; i++) {
        struct mem_map_entry* entry = &mem_map[i];

        if(entry->type == region_type_normal) {
            available_memory += entry->length;
        }

        print_mem_entry(i, entry->base, entry->length, get_mem_type(entry->type));
    }

    uint32_t kernel_end = (uint32_t)&LD_KERNEL_END;
    uint32_t kernel_start = (uint32_t)&LD_KERNEL_START;
    uint32_t kernel_size = kernel_end - kernel_start;

    print_mem_entry(mem_entry_count, kernel_start, kernel_size, "KERNEL");

    print_available_memory(available_memory);
}

// -------------------------------------------------------------------------
// Static Functions
// -------------------------------------------------------------------------
static void print_available_memory(uint64_t available_memory)
{
    terminal_write_string("Total amount of usable memory: ");

    uint32_t kb = available_memory / 1024;
    uint32_t mb = kb < 1024 ? 0 : kb / 1024;
    uint32_t gb = mb < 1024 ? 0 : mb / 1024;

    if(gb > 0) {
        terminal_write_uint32(gb);
        terminal_write_string("GiB");
    }
    else if(mb > 0) {
        terminal_write_uint32(mb);
        terminal_write_string("MiB");
    }
    else if (kb > 0) {
        terminal_write_uint32(kb);
        terminal_write_string("KiB");
    }
    else {
        terminal_write_string("Get a new PC, friend...");
    }

    terminal_write_string("\n");
}

static void print_mem_entry(size_t index, uint64_t base, uint64_t length, char* description)
{
    uint64_t end_address = base + length;
    terminal_write_string("[");
    terminal_write_uint32(index);
    terminal_write_string("] ");

    if(base < UINT16_MAXVALUE && end_address < UINT16_MAXVALUE) {
        terminal_write_uint16_x((uint16_t)base);
        terminal_write_string(" -> ");
        terminal_write_uint16_x((uint16_t)end_address);
    }
    else if(base < UINT32_MAXVALUE && end_address < UINT32_MAXVALUE) {
        terminal_write_uint32_x((uint32_t)base);
        terminal_write_string(" -> ");
        terminal_write_uint32_x((uint32_t)end_address);
    }
    else {
        terminal_write_uint64_x(base);
        terminal_write_string(" -> ");
        terminal_write_uint64_x(end_address);
    }

    terminal_write_string(" (");
    terminal_write_string(description);
    terminal_write_string(")\n");
}

static char* get_mem_type(enum region_type type)
{
    switch(type) {
        case region_type_normal:
            return "Available";
        case region_type_reserved:
            return "Reserved";
        case region_type_acpi_reclaimable:
            return "ACPI Reclaimable";
        case region_type_acpi_nvs:
            return "ACHPI NVS";
        case region_type_bad:
            return "Bad";
        default:
            return "Unknown";
    }
}
