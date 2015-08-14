#include <types.h>
#include <kernel.h>
#include <paging.h>
#include <mem_mgr.h>
#include <terminal.h>

#define PDE_32_PT_ADDRESS_MASK (0xFFFFF000)
#define PTE_32_4K_ADDRESS_MASK (0xFFFFF000)

// Page Directory Entry
// 32 <----------> 12  11:8  7   6   5   4    3    2    1   0
// Page Table Address  resv  0  ign  a  pcd  pwt  u/s  r/w  p
enum pde_32_pt {
    pde_32_pt_present       = 1 << 0, // p
    pde_32_pt_read_write    = 1 << 1, // r/w
    pde_32_pt_user          = 1 << 2, // u/s
    pde_32_pt_write_through = 1 << 3, // pwt
    pde_32_pt_cache_disable = 1 << 4, // pcd
    pde_32_pt_accessed      = 1 << 5, // a
    pde_32_pt_ign           = 1 << 6  // ign
};

// Page Table Entry
// 32 <----------> 12   11:9    8  7   6  5  4   3   2  1 0
// Page Frame Address   resv    g PAT  d  a pcd pwt us rw p
enum pte_32_4k {
    pte_32_4k_present       = 1 << 0, // p
    pte_32_4k_read_write    = 1 << 1, // rw
    pte_32_4k_user          = 1 << 2, // us
    pte_32_4k_write_through = 1 << 3, // pwt
    pte_32_4k_cache_disable = 1 << 4, // pcd
    pte_32_4k_accessed      = 1 << 5, // a
    pte_32_4k_dirty         = 1 << 6, // d
    pte_32_4k_pat           = 1 << 7, // pat
    pte_32_4k_global        = 1 << 8  // g
};

void print_pde(uintptr_t pd, uint16_t entry_index)
{
    uint32_t pde = ((uint32_t*)pd)[entry_index];
    terminal_write_string("PDE [");
    terminal_write_uint32(entry_index);
    terminal_write_string("]: ");
    terminal_write_uint32_x(pde & PDE_32_PT_ADDRESS_MASK);
    terminal_write_string(" Flags: ");

    bool present = (pde & pde_32_pt_present) == pde_32_pt_present;
    bool rw = (pde & pde_32_pt_read_write) == pde_32_pt_read_write;
    bool user = (pde & pde_32_pt_user) == pde_32_pt_user;
    bool write_through = (pde & pde_32_pt_write_through) == pde_32_pt_write_through;
    bool cache_disable = (pde & pde_32_pt_cache_disable) == pde_32_pt_cache_disable;
    bool accessed = (pde & pde_32_pt_accessed) == pde_32_pt_accessed;

    terminal_write_string(present ? "P " : "p ");
    terminal_write_string(rw ? "R " : "r ");
    terminal_write_string(user ? "U " : "u ");
    terminal_write_string(write_through ? "W " : "w ");
    terminal_write_string(cache_disable ? "C " : "c ");
    terminal_write_string(accessed ? "A " : "a ");
    terminal_write_string("\n");
}

void print_pte(uintptr_t pt, uint16_t entry_index)
{
    uint32_t pte = ((uint32_t*)pt)[entry_index];
    terminal_write_string("PTE [");
    terminal_write_uint32(entry_index);
    terminal_write_string("]: ");
    terminal_write_uint32_x(pte & PTE_32_4K_ADDRESS_MASK);
    terminal_write_string(" Flags: ");

    bool present = (pte & pte_32_4k_present) == pte_32_4k_present;
    bool rw = (pte & pte_32_4k_read_write) == pte_32_4k_read_write;
    bool user = (pte & pte_32_4k_user) == pte_32_4k_user;
    bool write_through = (pte & pte_32_4k_write_through) == pte_32_4k_write_through;
    bool cache_disable = (pte & pte_32_4k_cache_disable) == pte_32_4k_cache_disable;
    bool accessed = (pte & pte_32_4k_accessed) == pte_32_4k_accessed;
    bool dirty = (pte & pte_32_4k_dirty) == pte_32_4k_dirty;
    bool pat = (pte & pte_32_4k_pat) == pte_32_4k_pat;
    bool global = (pte & pte_32_4k_global) == pte_32_4k_global;

    terminal_write_string(present ? "P " : "p ");
    terminal_write_string(rw ? "R " : "r ");
    terminal_write_string(user ? "U " : "u ");
    terminal_write_string(write_through ? "W " : "w ");
    terminal_write_string(cache_disable ? "C " : "c ");
    terminal_write_string(accessed ? "A " : "a ");
    terminal_write_string(dirty ? "D " : "d ");
    terminal_write_string(pat ? "T " : "t ");
    terminal_write_string(global ? "G " : "g ");
    terminal_write_string("\n");
}

uintptr_t page_directory_create()
{
    // Page Directory fits neatly inside a page, neat!
    uintptr_t pd = (uintptr_t)mem_page_get();

    // Make sure our frame is nice and clean
    for(size_t i = 0; i < 1024; i++) {
        *((uint32_t*)(uintptr_t)pd) = 0;
    }

    // Allocate kernel code (Read-Only, Supervisor, cache enabled)
    struct mem_region* kcode = mem_get_kernel_region(mem_region_type_code);
    for(size_t i = 0; i < kcode->size / PAGE_SIZE; i++) {
        uint32_t page_address = kcode->start + (i * PAGE_SIZE);
        page_allocate(pd, page_address, page_address, pde_32_pt_write_through);
    }

    // Allocate kernel Read-Only data (Read-Only, Supervisor, cache enabled)
    struct mem_region* krodata = mem_get_kernel_region(mem_region_type_rodata);
    for(size_t i = 0; i < krodata->size / PAGE_SIZE; i++) {
        uint32_t page_address = krodata->start + (i * PAGE_SIZE);
        page_allocate(pd, page_address, page_address, pde_32_pt_write_through);
    }

    // Allocate kernel Read-Only data (Read-Only, Supervisor, cache enabled)
    struct mem_region* krwdata = mem_get_kernel_region(mem_region_type_data);
    for(size_t i = 0; i < krwdata->size / PAGE_SIZE; i++) {
        uint32_t page_address = krwdata->start + (i * PAGE_SIZE);
        page_allocate(pd, page_address, page_address, pde_32_pt_write_through | pde_32_pt_read_write);
    }

    return pd;
}

void page_allocate(uintptr_t pd_ptr, uintptr_t physical_addr, uintptr_t virtual_addr, uint32_t attr)
{
    // Virtual address breakdown:
    // 31:22 - Directory in pde_ptr
    // 21:12 - Table entry in Directory found in prev step
    // 11:0  - Offset within table enter (we don't touch this)
    uint16_t pde_index = (virtual_addr >> 22) & 0x3FF;

    uint32_t* pde_ptr = (uint32_t*)(pd_ptr + pde_index);
    if((*pde_ptr & pde_32_pt_present) != pde_32_pt_present) {

        // Initialize PDE with a brand new page for the PT
        uint32_t pt = (uint32_t)(intptr_t)mem_page_get();

        // Make sure the frame is nice and clean
        for(size_t i = 0; i < 1024; i++) {
            *((uint32_t*)(uintptr_t)pt + i) = 0;
        }

        *pde_ptr = (pt & PDE_32_PT_ADDRESS_MASK) | pde_32_pt_present | attr;
    }

    uint32_t* pt = (uint32_t*)(uintptr_t)(*pde_ptr & PDE_32_PT_ADDRESS_MASK);
    uint16_t pte_index = (((uint32_t)virtual_addr) >> 12) & 0x3FF;
    uint32_t* pte = (pt + pte_index);

    if((*pte & pte_32_4k_present) != pte_32_4k_present) {

        // Page table entry not initialized, do it
        *pte = (physical_addr & PTE_32_4K_ADDRESS_MASK) | pte_32_4k_present | attr;
    }
    else {
        KERROR("Attempted to allocate page that is already allocated");
    }
}

