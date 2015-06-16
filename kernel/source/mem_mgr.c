#include <kernel.h>
#include <types.h>
#include <mem_mgr.h>
#include <terminal.h>

struct mem_map_entry** g_mem_map;
uint32_t g_mem_map_entries;

static char* get_mem_type(enum region_type type);

void mem_mgr_init(struct mem_map_entry mem_map[], uint32_t mem_entry_count)
{
    g_mem_map = &mem_map;
    g_mem_map_entries = mem_entry_count;

    if(mem_entry_count == 0) {
        terminal_write_string("Surprisignly enough, we have all of memory to ourselves!\n");
        return;
    }

    terminal_write_string("Processing memory map...\n");
    terminal_write_string("Reserved memory regions:\n");

    for (int i = 0; i < mem_entry_count; i++) {
        struct mem_map_entry* entry = &mem_map[i];

        terminal_write_string("[");
        terminal_write_uint32(i);
        terminal_write_string("] ");

        if(entry->base < UINT16_MAXVALUE) {
            terminal_write_uint16_x((uint16_t)entry->base);
            terminal_write_string(" -> ");
            terminal_write_uint16_x((uint16_t)(entry->base + entry->length));
        }
        else if(entry->base < UINT32_MAXVALUE) {
            terminal_write_uint32_x((uint32_t)entry->base);
            terminal_write_string(" -> ");
            terminal_write_uint32_x((uint32_t)(entry->base + entry->length));
        }
        else {
            terminal_write_uint64_x(entry->base);
            terminal_write_string(" -> ");
            terminal_write_uint64_x(entry->base + entry->length);
        }

        terminal_write_string(" (");
        terminal_write_string(get_mem_type(mem_map[i].type));
        terminal_write_string(")\n");
    }
}

static char* get_mem_type(enum region_type type)
{
    switch(type) {
        case region_type_normal:
            return "Normal";
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
