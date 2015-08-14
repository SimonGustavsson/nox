#ifndef NOX_PAGING_H
#define NOX_PAGING_H

uintptr_t page_directory_create();
void page_allocate(uintptr_t pde_ptr, uintptr_t physical_addr, uintptr_t virtual_addr, uint32_t attr);
void print_pde(uintptr_t pd, uint16_t entry_index);
void print_pte(uintptr_t pt, uint16_t entry_index);
void page_directory_install(uintptr_t pd);

#endif
