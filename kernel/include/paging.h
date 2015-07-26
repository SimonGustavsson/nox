#ifndef NOX_PAGING_H
#define NOX_PAGING_H

void page_allocate(intptr_t pde_ptr, intptr_t physical_addr, intptr_t virtual_addr, uint32_t attr);

#endif
