#include <kernel.h>
#include <types.h>
#include <mem_mgr.h>
#include <terminal.h>
#include <debug.h>

//#define GDT_DEBUG
//#define MEM_DEBUG

// -------------------------------------------------------------------------
// Static Defines
// -------------------------------------------------------------------------

#define PAGE_USED (1 << 7)
#define FIRST_IN_ALLOCATION (1 << 6)
#define PAGE_RESERVED (1 << 5)
#define IS_PAGE_USED(x) ((x & PAGE_USED) == PAGE_USED)
#define IS_FIRST_IN_ALLOCATION(x) ((x & FIRST_IN_ALLOCATION) == FIRST_IN_ALLOCATION)
#define IS_PAGE_RESERVED(x) ((x & PAGE_RESERVED) == PAGE_RESERVED)

#define NYBL(TargetType, Value, AtBit) ((((TargetType)Value) >> AtBit) & 0x0F)
#define BYTE(TargetType, Value, AtBit) ((((TargetType)Value) >> AtBit) & 0xFF)
#define WORD(TargetType, Value, AtBit) ((((TargetType)Value) >> AtBit) & 0xFFFF)
#define GDT_ENTRY(Limit, Base, Access, Flags) GDT_ENTRY_(Limit, Base, (Access | gdt_access_type), ((Flags | gdt_flag_4k) | gdt_flag_32bit))
#define GDT_ENTRY_(Limit, Base, Access, Flags) ( \
            WORD(uint64_t, Limit,  0)   << 00 |  \
            WORD(uint64_t, Base,   0)   << 16 |  \
            BYTE(uint64_t, Base,   16)  << 32 |  \
            BYTE(uint64_t, Access, 0)   << 40 |  \
            NYBL(uint64_t, Limit,  16)  << 48 |  \
            NYBL(uint64_t, Flags,  0)   << 52 |  \
            BYTE(uint64_t, Base,   24)  << 56 )

// -------------------------------------------------------------------------
// Static Types
// -------------------------------------------------------------------------
struct page {
    uint8_t flags;

    // Note: The fact that this is an uint16 means page allocations
    //       are limited to UINT16_MAXVALUE, which is roughly 256MB
    uint16_t consecutive_pages_allocated;
} PACKED;

struct gtdd {
    uint16_t size;   // Size of table - 1
    uint32_t offset; // Address in linear address space
} PACKED;

enum gdt_flag {
    // Granularity. Not set means 'limit' is in 1b blocks. Set = limit is in pages
    gdt_flag_4k = 1 << 3,
    gdt_flag_32bit = 1 << 2
};

enum gdt_access {
    gdt_access_present        = 1 << 7,
    gdt_access_priv_ring2     = 2 << 5,
    gdt_access_priv_ring3     = 3 << 5,
    gdt_access_type           = 1 << 4, // Set for all code/data segments
    gdt_access_executable     = 1 << 3,
    gdt_access_direction_down = 1 << 2,
    gdt_access_rw             = 1 << 1,
    gdt_access_accessed       = 1,
};

// Stores data for a task, in our case
// we only have one task which we use
// for all user-mode code. Its only
// utility is so that INT can determine
// what stack to setup when switching
// back into kernel mode
struct tss {

    // This field is only useful for
    // hardware task switching, where
    // it's a link to the previous task
    struct {
        uint32_t link;
    } PACKED unused0;

    // The stack pointer and stack segment to load when switching
    // to ring 0 from the current task
    uint32_t esp0;
    uint32_t ss0;

    // The following fields are only relevant for
    // hardware task switching
    struct {
        uint32_t esp1;
        uint32_t ss1;
        uint32_t esp2;
        uint32_t ss2;
        uint32_t cr3;
        uint32_t eip;
        uint32_t eflags;
        uint32_t eax;
        uint32_t ecx;
        uint32_t edx;
        uint32_t ebx;
        uint32_t esp;
        uint32_t ebp;
        uint32_t esi;
        uint32_t edi;
        uint32_t es;
        uint32_t cs;
        uint32_t ss;
        uint32_t ds;
        uint32_t fs;
        uint32_t gs;
        uint32_t ldtr;
        uint16_t reserved;
    } PACKED unused1;

    uint16_t iopb;
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

// Global descriptor table
static uint64_t g_gdt[] ALIGN(8) = {
    GDT_ENTRY_(0, 0, 0, 0),
    GDT_ENTRY(0x00FFFFFF, 0x00000000, gdt_access_rw | gdt_access_present | gdt_access_executable, 0),
    GDT_ENTRY(0x00FFFFFF, 0x00000000, gdt_access_rw | gdt_access_present, 0),
    GDT_ENTRY(0x00FFFFFF, 0x00000000, gdt_access_rw | gdt_access_present | gdt_access_priv_ring3 | gdt_access_executable, 0),
    GDT_ENTRY(0x00FFFFFF, 0x00000000, gdt_access_rw | gdt_access_present | gdt_access_priv_ring3, 0),
    // Reserved space for TSS
    GDT_ENTRY_(0, 0, 0, 0)
};

static size_t g_gdt_count = sizeof(g_gdt) / sizeof(uint64_t);

static struct gtdd g_gtdd ALIGN(8);

#define TSS_GTD_TYPE (0x89)
#define TSS_GDTD_INDEX (g_gdt_count - 1)
static struct tss g_tss ALIGN(8);

// -------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------
static char* get_mem_type(enum region_type type);
static void print_memory_nice(uint64_t memory_in_bytes);
static void print_mem_entry(size_t index, uint64_t base, uint64_t length, char* description);
static void test_allocator();
static void tss_install();
static void gdt_install();
static void clear_page(uint32_t* page_addr);

#ifdef GDT_DEBUG
void print_gdt()
{
    terminal_write_string("Global Descriptor Table: \n");
    terminal_indentation_increase();
    for (int i = 0; i < g_gdt_count; i++) {
        terminal_write_char('[');
        terminal_write_uint32(i);
        terminal_write_string("] ");
        terminal_write_uint64_bytes(g_gdt[i]);
        terminal_write_string(".\n");
    }
    terminal_indentation_decrease();
}
#endif

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

    terminal_write_string("Calculating total memory...");
    for (int i = 0; i < mem_entry_count; i++) {
        struct mem_map_entry* entry = &mem_map[i];

        if(entry->type == region_type_normal) {
            g_total_available_memory += entry->length;
        }

        print_mem_entry(i, entry->base, entry->length, get_mem_type(entry->type));
    }

    g_max_pages = g_total_available_memory / PAGE_SIZE;

    uint32_t kernel_end = (uint32_t)(intptr_t)&LD_KERNEL_END;
    uint32_t kernel_start = (uint32_t)(intptr_t)&LD_KERNEL_START;
    uint32_t kernel_size = kernel_end - kernel_start;

    uint32_t kernel_start_page = (kernel_start / PAGE_SIZE);
    if(kernel_start % PAGE_SIZE != 0)
        kernel_start_page++;

    uint32_t kernel_pages = kernel_size / PAGE_SIZE;
    if(kernel_size % PAGE_SIZE != 0)
      kernel_pages++;

    // ~We put the page map right after the kernel in memory~
    // Put it at 10 MB, that ought to never cause any load issues (RIGHT!?)

    g_pages = (struct page*)(intptr_t) 0x3200000;//(kernel_start + (kernel_pages * PAGE_SIZE));

    // Reserve pages for the page map itself
    size_t max_pages = g_total_available_memory / PAGE_SIZE;
    size_t mem_map_size = (max_pages * sizeof(struct page)) ;
    size_t mem_map_pages = mem_map_size / PAGE_SIZE;
    if(mem_map_size % PAGE_SIZE != 0)
        mem_map_pages++;


    // Zero out the memory map
    for(size_t i = 0; i < g_max_pages; i++) {
        g_pages[i].flags = 0;
        g_pages[i].consecutive_pages_allocated = 0;
    }

    // Reserve the pages we know about right now
    // (Note: -1 because the first page at kernel_start belongs to the kernel)
    uint32_t pages_before_kernel = (kernel_start / PAGE_SIZE) - 1;
    if (kernel_start % PAGE_SIZE) pages_before_kernel += 1;

    if (!mem_page_reserve("BIOS/hardware", (void*)0x0, pages_before_kernel))
        KPANIC("Failed to reserve low-memory for bios/hardware");
    if (!mem_page_reserve("Kernel", (void*)kernel_start, kernel_pages))
        KPANIC("Unable to reserve kernel memory in page allocator");
    if (!mem_page_reserve("PAGES", (void*)g_pages, mem_map_pages))
        KPANIC("Failed to reserve pages for memory allocator!");

    // And just a quick test to make sure everything works
    test_allocator();

    terminal_write_string("Reserved ");
    mem_print_usage();

    // All done with pages. Setup GDT last (as it relies on allocating pages)
    mem_mgr_gdt_setup();
}

void mem_mgr_gdt_setup()
{
    // Set up the TSS
    g_tss.ss0 = 0x10; // Kernel data-segment selector

    // ISR stack is ten pages, might want to make bigger
    uint32_t num_stack_pages = 10;
    g_tss.esp0 = (uint32_t)(intptr_t)(mem_page_get_many(num_stack_pages) + (PAGE_SIZE * num_stack_pages));

    // This is the index from the start of the TSS of the IO
    // Port Bitmap - our limit for the TSS in the GDT
    // is the end of the TSS so that basically means there is
    // no IO Port Bitmap at all. Chapter 16, Volume 1 of the
    // x86 Developer Guide says that the bitmap may be partial,
    // so this is entirely okay.
    g_tss.iopb = sizeof(struct tss);

    uint32_t tss_base = (uint32_t)(intptr_t)(&g_tss);
    uint32_t tss_size = sizeof(struct tss);

    // TSS Access byte is different from normal GDT entries:
    // 0: Always 1
    // 1   -  Busy Flag (0 in our case, as it must be available)
    // 2   - Always 0
    // 3   - Always 1
    // 4   - Always 0
    // 4:5 - DPL
    // 6   - Present (Must be 1)
    //
    // This means the base value is 1~~01001, with '~~' being the priv level
    // (The TSS flag nybble is a bit different as well, but nothing that is relevant to us)
    g_gdt[TSS_GDTD_INDEX] = GDT_ENTRY_(tss_size - 1, tss_base, 0x89, 0);

    // This is what it is at runtime: 6700 805A 01E9 0000
    // Base: 015A80
    // Limit: 0067
    // Access: E9
    // Flags: 0

#ifdef GDT_DEBUG
    print_gdt();
#endif

    g_gtdd.size = (sizeof(uint64_t) * g_gdt_count) - 1;
    g_gtdd.offset = (uint32_t)(intptr_t)&g_gdt;

    gdt_install();

    tss_install();
}

uint64_t gdte_create(uint32_t limit, uint32_t base, uint8_t access, enum gdt_flag flags)
{
    return GDT_ENTRY(limit, base, access, flags);
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
    terminal_write_string(" pages)\n");
}

bool mem_page_reserve(const char* identifier, void* address, size_t num_pages)
{
#ifdef MEM_DEBUG
    terminal_write_string("Reserving ");
    terminal_write_uint32(num_pages);
    terminal_write_string(" pages for '");
    terminal_write_string(identifier);
    terminal_write_string("' at ");
    terminal_write_uint32_x(((uint32_t)address / PAGE_SIZE) * PAGE_SIZE);
    terminal_write_char('-');
    terminal_write_uint32_x(((uint32_t)address) + (((num_pages - 1) * PAGE_SIZE)));
    terminal_write_string(" (");
    terminal_write_uint32(num_pages * PAGE_SIZE);
    terminal_write_string(" bytes)\n");
#endif

    size_t page_index = ((size_t)(intptr_t)(address)) / PAGE_SIZE;
    if(page_index < 0 || page_index > g_max_pages)
    {
        KWARN("An attempt was made to reserve a page outside of memory");
        return false;
    }

    if(IS_PAGE_RESERVED(g_pages[page_index].flags)) {
        KWARN("An attempt was made to re-reserve a page!");
        SHOWVAL_U32("The following page is already reserved: ", (uint32_t)(intptr_t)address);
        return false;
    }

    if(num_pages == 1) {
        g_pages[page_index].flags = PAGE_USED | PAGE_RESERVED;
        return true;
    }

    // Make sure all requested pages are available
    for(size_t i = 0; i < num_pages; i++) {
        struct page* cur = &g_pages[page_index + 1];

        if(IS_PAGE_USED(cur->flags) || IS_PAGE_RESERVED(cur->flags)) {
            KPANIC("That entire block is not available!");
            return false;
        }
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
    if (how_many == 1) {
        return mem_page_get();
    }

    // THe first page is a given, we knew that...
    how_many--;

    for(size_t i = 0; i < g_max_pages; i++) {

        // Do we have how_many free pages after this?
        size_t free_found = 0;
        for (int k = 0; k < how_many && i + k < g_max_pages; k++) {
            int page_num = i + k;
            struct page* cur = &g_pages[page_num];

            if(IS_PAGE_USED(cur->flags) || IS_FIRST_IN_ALLOCATION(cur->flags)) {
                free_found = 0;
                break;
            } else {
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

            // SHOWVAL_U32("Marking page as used: ", i + j);

            uint32_t* page_ptr = (uint32_t*) ((i+j) * PAGE_SIZE);
            clear_page(page_ptr);
        }

#ifdef MEM_DEBUG
        terminal_write_string("Allocated ");
        terminal_write_uint32(how_many + 1);
        terminal_write_string(" pages @ ");
        terminal_write_uint32_x((uint32_t)(i * PAGE_SIZE));
        terminal_write_char('\n');
#endif

        return (void*)(intptr_t)(i * PAGE_SIZE);
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

#ifdef MEM_DEBUG
            SHOWVAL_U32("Allocated new page: ", (uint32_t)(intptr_t)(i * PAGE_SIZE));
#endif

            uint32_t page_addr = i * PAGE_SIZE;

            clear_page((uint32_t*)page_addr);

            return (void*)page_addr;
        }
    }

    return NULL;
}

void mem_page_free(void* address)
{
#ifdef MEM_DEBUG
    terminal_write_string("page_free: ");
    terminal_write_uint32(((uint32_t)address) / PAGE_SIZE);
    terminal_write_string(" @ " );
    terminal_write_uint32_x((uint32_t)address);
    terminal_write_char('\n');
#endif

    if(address == NULL) {
        KPANIC("NULL pointer passed to mem_page_free");
        return;
    }

    size_t page_index = ((size_t)(intptr_t)(address)) / PAGE_SIZE;
    if(page_index < 0 || page_index > g_max_pages)
        return;

    struct page* cur = &g_pages[page_index];
    if(!IS_FIRST_IN_ALLOCATION(cur->flags)) {
        terminal_write_string("Invalid call to page_free(");
        terminal_write_uint32_x((uint32_t)(intptr_t)address);
        terminal_write_string(" not first page!\n");
        return; // This is not the first page of an allocation
    }

    if(IS_PAGE_RESERVED(cur->flags)) {
        KERROR("Tried to free reserved page!");
        return;
    }

    size_t pages_left = cur->consecutive_pages_allocated;
    cur->flags = 0;
    cur->consecutive_pages_allocated = 0;
    cur++;

#ifdef MEM_DEBUG
    terminal_write_string("Freeing ");
    terminal_write_uint32(pages_left);
    terminal_write_string(" pages also in allocation\n");
#endif

    // Free the first page
    // Free the consecutive pages
    for(size_t i = 0; i < pages_left; i++) {
#ifdef MEM_DEBUG
        SHOWVAL_U32("Freeing page: ", (page_index + i) * PAGE_SIZE);
#endif
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

        mem_print_usage();

        KPANIC("mem_init_check_failed");
    }
}

static void clear_page(uint32_t* page_addr)
{
    for (int i = 0; i < 1024; i++) {
        *(page_addr + i) = 0;
    }
}

static void gdt_install()
{
#ifdef GDT_DEBUG
    KINFO("Installing GDT");
    SHOWVAL_x("GTDD Address: ", (uint32_t)(intptr_t)&g_gtdd);
    SHOWVAL("Size: ", g_gtdd.size);
    SHOWVAL_x("GTD address: ", g_gtdd.offset);
#endif

    __asm ("mov %0  ,  %%ax;    \
            mov %%ax,  %%ds;    \
            mov %%ax,  %%es;    \
            mov %%ax,  %%fs;    \
            mov %%ax,  %%gs;    \
            mov %%ax,  %%ss;    \
            lgdt (%1);          \
            ljmp %2, $_gdt_loaded; \
            _gdt_loaded:"
            :
            : "i" (KERNEL_DATA_SEGMENT),
              "m" (g_gtdd),
              "i" (KERNEL_CODE_SEGMENT)
            );
}

static void tss_install()
{
#ifdef GDT_DEBUG
    terminal_write_string("Installing LDT: ");
    terminal_write_uint64_x(g_gdt[TSS_GDTD_INDEX]);
    terminal_write_char('\n');
#endif

    __asm("ltr %%ax" :: "a" (0x28));
}

