#include <types.h>
#include <kernel.h>
#include <paging.h>
#include <mem_mgr.h>

#define PDE_32_PT_ADDRESS_MASK (0xFFFFFC00)

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
    pte_32_4k_readwrite     = 1 << 1, // rw
    pte_32_4k_user          = 1 << 2, // us
    pte_32_4k_write_through = 1 << 3, // pwt
    pte_32_4k_cache_disable = 1 << 4, // pcd
    pte_32_4k_accessed      = 1 << 5, // a
    pte_32_4k_dirty         = 1 << 6, // d
    pte_32_4k_pat           = 1 << 7, // pat
    pte_32_4k_global        = 1 << 8  // g
};

uintptr_t page_directory_create()
{
    // Page Directory fits neatly inside a page, neat!
    uintptr_t pd = (uintptr_t)mem_page_get();

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

void page_allocate(intptr_t pd_ptr, intptr_t physical_addr, intptr_t virtual_addr, uint32_t attr)
{
    // Virtual address breakdown:
    // 31:22 - Directory in pde_ptr
    // 21:12 - Table entry in Directory found in prev step
    // 11:0  - Offset within table enter (we don't touch this)
    uint16_t pde_index = (virtual_addr >> 22) & 0xFFFF;

    uint32_t* pde_ptr = (uint32_t*)(pd_ptr + pde_index);
    if((*pde_ptr & pde_32_pt_present) == 0) {

        // Initialize PDE with a brand new page for the PT
        uint32_t pt = (uint32_t)(intptr_t)mem_page_get();
        *pde_ptr = (pt & PDE_32_PT_ADDRESS_MASK) | pde_32_pt_present | attr;
    }

    uint32_t* pt = (uint32_t*)(uintptr_t)(*pde_ptr & PDE_32_PT_ADDRESS_MASK);
    uint32_t* pte = (pt + pde_index);

    if((*pte & pte_32_4k_present) != pte_32_4k_present) {
        
        // Page table entry not initialized, do it
        *pte = 0/*Address here... */ | pte_32_4k_present | attr;
    }
}

