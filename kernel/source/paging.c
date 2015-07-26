#include <types.h>
#include <paging.h>

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
    pde_32_pt_ign           = 1 << 6, // ign
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

void page_init_table(intptr_t pde_ptr)
{
    // A virtual address has 10 bits for the page Directory Entry index (31:22)
    // there are 1024 PDEs in the root table, covering a total of 4096MiB of memory
    //
    // A virtual address has 10 bits for the Table Entry (21:12)
    // A page table has 1024 PDEs in the root, covering a total of 4MiB of memory

    // The address for a Page Table has to be aligned to a 1024 boundary, because
    // we only have 20-bits to store the address to it within the PDE    

    // Recap: All entries are 32-bit
    // There are 1024 PDEs
    // There are 1024 Page Tables
    // Each Page Table has 1024 entries
    // This means there are a total of 1048576 PTEs
    // = Each Page Table is 4MiB
}

void page_allocate(intptr_t pde_ptr, intptr_t physical_addr, intptr_t virtual_addr, uint32_t attr)
{
    // Virtual address breakdown:
    // 31:22 - Directory in pde_ptr
    // 21:12 - Table entry in Directory found in prev step
    // 11:0  - Offset within table enter (we don't touch this)
    uint16_t dir = (virtual_addr >> 22) & 0xFFFF;
    uint16_t table_index = (virtual_addr >> 12) & 0xFFF;

    uint32_t dir_entry = (uint32_t)(pde_ptr + dir);
    if((dir_entry & pde_32_pt_present) == 0) {

        // PDE has not been initialized yet
    }
}

