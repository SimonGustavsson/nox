#include <kernel.h>
#include <types.h>
#include <mem_mgr.h>
#include <terminal.h>

// -------------------------------------------------------------------------
// Static Defines
// -------------------------------------------------------------------------
#define PAGE_SIZE (4096)

#define PAGE_USED (1 << 7)
#define FIRST_IN_ALLOCATION (1 << 6)
#define PAGE_RESERVED (1 << 5)
#define IS_PAGE_USED(x) ((x & PAGE_USED) == PAGE_USED)
#define IS_FIRST_IN_ALLOCATION(x) ((x & FIRST_IN_ALLOCATION) == FIRST_IN_ALLOCATION)
#define IS_PAGE_RESERVED(x) ((x & PAGE_RESERVED) == PAGE_RESERVED)

// -------------------------------------------------------------------------
// Static Types
// -------------------------------------------------------------------------
struct page {
    uint8_t flags;

    // Note: The fact that this is an uint16 means page allocations
    //       are limited to UINT16_MAXVALUE, which is roughly 256MB
    uint16_t consecutive_pages_allocated;
} PACKED;

// -------------------------------------------------------------------------
// Global variables
// -------------------------------------------------------------------------
extern uint32_t LD_KERNEL_START;
extern uint32_t LD_KERNEL_DATA_END;
extern uint32_t LD_KERNEL_END;

// Memory map as given by the bootloader
static struct mem_map_entry** g_mem_map;
static uint32_t g_mem_map_entries;

// Map of all pages we can allocate
static struct page* g_pages;
static size_t g_max_pages;
static size_t g_total_available_memory;

// -------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------
static char* get_mem_type(enum region_type type);
static void print_memory_nice(uint64_t memory_in_bytes);
static void print_mem_entry(size_t index, uint64_t base, uint64_t length, char* description);
static void test_allocator();

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

    for (int i = 0; i < mem_entry_count; i++) {
        struct mem_map_entry* entry = &mem_map[i];

        if(entry->type == region_type_normal) {
            g_total_available_memory += entry->length;
        }

        if(1 == 0)
            print_mem_entry(i, entry->base, entry->length, get_mem_type(entry->type));
    }
    g_max_pages = g_total_available_memory / PAGE_SIZE;

    uint32_t kernel_end = (uint32_t)&LD_KERNEL_END;
    uint32_t kernel_start = (uint32_t)&LD_KERNEL_START;
    uint32_t kernel_size = kernel_end - kernel_start;

    terminal_write_string("Kernel address: ");
    terminal_write_uint32_x(kernel_start);
    terminal_write_string("->");
    terminal_write_uint32_x(kernel_end);
    terminal_write_char('\n');

    uint32_t kernel_start_page = (kernel_start / PAGE_SIZE);
    if(kernel_start % PAGE_SIZE != 0)
        kernel_start_page++;

    uint32_t kernel_pages = kernel_size / PAGE_SIZE;
    if(kernel_size % PAGE_SIZE != 0)
      kernel_pages++;

    // We put the page map right after the kernel in memory
    uint32_t mem_map_address = (kernel_start + (kernel_pages * PAGE_SIZE));

    terminal_write_string("Page map address: ");
    terminal_write_uint32_x(mem_map_address);
    terminal_write_string("->");

    g_pages = (struct page*)(kernel_start + (kernel_pages * PAGE_SIZE));

    // Reserve pages for the page map itself
    size_t max_pages = g_total_available_memory / PAGE_SIZE;
    size_t mem_map_size = (max_pages * sizeof(struct page)) ;
    size_t mem_map_pages = mem_map_size / PAGE_SIZE;
    if(mem_map_size % PAGE_SIZE != 0)
        mem_map_pages++;

    terminal_write_uint32_x(mem_map_address + (mem_map_pages * PAGE_SIZE) - 1);
    terminal_write_string("\n");

    // Zero out the memory map
    for(size_t i = 0; i < g_max_pages; i++) {
        g_pages[i].flags = 0;
        g_pages[i].consecutive_pages_allocated = 0;
    }

    // Reserve the pages we know about right now
    if(!mem_page_reserve((void*)0x0, 0x7C00 / PAGE_SIZE))
        KERROR("Failed to reserve BIOS pages");
    if(!mem_page_reserve((void*)g_pages, mem_map_pages))
        KERROR("Failed to reserve pages for the page map!");
    if(!mem_page_reserve((void*)kernel_start, kernel_pages))
        KERROR("Failed to reserve pages for the kernel!");

    // And just a quick test to make sure everything words
    test_allocator();

    terminal_write_string("Reserved ");
    mem_print_usage();
}

void mem_print_usage()
{
    size_t available_pages = mem_page_count(true);
    terminal_write_string("Usage: ");
    print_memory_nice(available_pages * PAGE_SIZE);
    terminal_write_char('/');
    print_memory_nice(g_total_available_memory);
    terminal_write_string(" (");
    terminal_write_uint32(mem_page_count(true));
    terminal_write_string("/");
    terminal_write_uint32(g_max_pages);
    terminal_write_string("pages)\n");
}

bool mem_page_reserve(void* address, size_t num_pages)
{
    size_t page_index = ((size_t)(address)) / PAGE_SIZE;
    if(page_index < 0 || page_index > g_max_pages)
    {
        KWARN("An attempt was made to reserve a page outside of memory");
        return false;
    }

    if(IS_PAGE_RESERVED(g_pages[page_index].flags)) {
        KWARN("An attempt was made to re-reserve a page!");
        return false;
    }

    if(num_pages == 1) {
        g_pages[page_index].flags = PAGE_USED | PAGE_RESERVED;
        return true;
    }

    // Make sure all requested pages are available
    for(size_t i = 0; i < num_pages; i++) {
        struct page* cur = &g_pages[page_index + 1];

        if(IS_PAGE_USED(cur->flags) || IS_PAGE_RESERVED(cur->flags))
            return false;
    }

    // We now know for a fact all is available, just mark them
    g_pages[page_index].flags = PAGE_USED | PAGE_RESERVED;

    for(size_t i = 0; i < num_pages; i++) {
        g_pages[page_index + i].flags = PAGE_USED | PAGE_RESERVED;
    }

    return true;
}

size_t mem_page_count(bool get_allocated)
{
    size_t result = 0;
    for(size_t i = 0; i < g_max_pages; i++) {
        bool is_used = IS_PAGE_USED(g_pages[i].flags);
        if((get_allocated && is_used) ||(!get_allocated && !is_used))
            result++;
    }

    return result;
}

void* mem_page_get_many(uint16_t how_many)
{
    // THe first page is a given, we knew that...
    how_many--;

    for(size_t i = 0; i < g_max_pages; i++) {
        struct page* cur = &g_pages[i];

        if(IS_PAGE_USED(cur->flags) || IS_FIRST_IN_ALLOCATION(cur->flags))
            continue;

        // Do we have how_many free pages after this?
        size_t free_found = 0;
        while(free_found < how_many &&  ++cur) {
            if(!IS_PAGE_USED(cur->flags) && !IS_FIRST_IN_ALLOCATION(cur->flags)) {
                free_found++;
            }
        }

        if(free_found != how_many) {
            continue;
        }

        g_pages[i].flags = PAGE_USED | FIRST_IN_ALLOCATION;
        g_pages[i].consecutive_pages_allocated = how_many;
        for(size_t j = 0; j < how_many; j++) {
            g_pages[i + j].flags |= PAGE_USED;
        }

        return (void*)(i * PAGE_SIZE);
    }

    KWARN("No pages available!");

    return NULL;
}

void* mem_page_get()
{
    for(size_t i = 0; i < g_max_pages; i++) {
        struct page* cur = &g_pages[i];

        if(!IS_PAGE_USED(cur->flags) && ! IS_FIRST_IN_ALLOCATION(cur->flags)) {
            cur->flags = PAGE_USED | FIRST_IN_ALLOCATION;

            return (void*)(i * PAGE_SIZE);
        }
    }

    return NULL;
}

void mem_page_free(void* address)
{
    if(address == NULL) {
        KWARN("NULL pointer passed to mem_page_free");
        return;
    }

    size_t page_index = ((size_t)(address)) / PAGE_SIZE;
    if(page_index < 0 || page_index > g_max_pages)
        return;

    struct page* cur = &g_pages[page_index];
    if(!IS_FIRST_IN_ALLOCATION(cur->flags)) {
        terminal_write_string("Invalid call to page_free(");
        terminal_write_uint32_x((uint32_t)address);
        terminal_write_string(" not first page!\n");
        return; // This is not the first page of an allocation
    }

    if(IS_PAGE_RESERVED(cur->flags)) {
        KERROR("Tried to free reserved page!");
        return;
    }

    size_t pages_left = cur->consecutive_pages_allocated;

    // Free the first page
    cur->flags = 0;
    cur->consecutive_pages_allocated = 0;
    cur++;
    // Free the consecutive pages
    for(size_t i = 0; i < pages_left; i++) {
        g_pages[page_index + i].flags = 0;
    }
}

// -------------------------------------------------------------------------
// Static Functions
// -------------------------------------------------------------------------
static void print_memory_nice(uint64_t memory_in_bytes)
{
    uint32_t kb = memory_in_bytes / 1024;
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
        terminal_write_uint32(memory_in_bytes);

        if(memory_in_bytes > 0)
            terminal_write_char('B');
    }
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

static void test_allocator()
{
    // This function is probably not necessary any more,
    // I found it useful whilst writing the stuff above as a sanity
    // checker though, so I'm leaving it in for now
    size_t allocated_pages = mem_page_count(true);
    void* allocations[10];
    allocations[9] = mem_page_get_many(14);
    if(allocations[9] == NULL) {
        KWARN("Failed to allocate 14 pages!");
    }

    for(uint32_t i = 0; i < 9; i++)
        allocations[i] = mem_page_get();

    for(uint32_t i = 0; i < 10; i++)
    {
        mem_page_free(allocations[i]);
    }
    size_t allocated_pages_after = mem_page_count(true);

    if(allocated_pages == allocated_pages_after) {
        terminal_write_string("Physical page allocator initialized\n");
    }
    else {
        terminal_write_string("Didn't deallocate all pages, before: ");
        terminal_write_uint32(allocated_pages);
        terminal_write_string(", after: ");
        terminal_write_uint32(allocated_pages_after);
        terminal_write_char('\n');
    }
}

