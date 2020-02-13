#include "memory.h"
#include "util.h"
#include "types.h"
#include "mem_mgr.h"
#include "terminal.h"

// See noxrust.a
struct Allocator;

extern struct Allocator* rust_create_allocator(uint8_t* base_addr, uint32_t max_bytes);
extern uint32_t* rust_alloc(struct Allocator* allocator, uint32_t size);
extern uint32_t* rust_aligned_alloc(struct Allocator* allocator, uint32_t size, uint8_t alignment);
extern uint32_t* rust_free(struct Allocator* allocator, uint32_t* ptr);

struct Allocator* g_allocator;
uint32_t* g_pages;

void memory_init()
{
    uint32_t pages_to_allocate = MAX_ALLOCATED_BYTES / PAGE_SIZE;
    pages_to_allocate += 1; // 1 page for the bitmap
    g_pages = (uint32_t*) mem_page_get_many(pages_to_allocate);

    g_allocator = rust_create_allocator((uint8_t*)g_pages, MAX_ALLOCATED_BYTES);
}

void* realloc(void* ptr, unsigned int size)
{
    // TOOD: Migrate implementation to Rust
    if (size == 0)
    {
        if (ptr != NULL)
        {
            rust_free(g_allocator, (uint32_t*) ptr);
        }

        return NULL;
    }
    else
    {
        void* new_ptr = rust_alloc(g_allocator, size);
        if (new_ptr == NULL)
            return NULL;

        if (ptr != NULL)
        {
            char* char_ptr = (char*)ptr;

            //How large is the allocation of the passed in ptr?
            unsigned int num_slices = (*(char_ptr - 4) << 24) |
                (*(char_ptr - 3) << 16) |
                (*(char_ptr - 2) << 8) |
                *(char_ptr - 1);

            unsigned int allocated_bytes = num_slices / BYTES_PER_SLICE;
            printf("realloc: Old size: %d - New Size: %d\n", allocated_bytes, size);

            // Copy over the old data
            unsigned int bytes_to_copy = 0;

            if (allocated_bytes >= size)
                bytes_to_copy = size;
            else
                bytes_to_copy = allocated_bytes;

            printf("Copying %d bytes from previous allocation.\n", bytes_to_copy);

            my_memcpy(new_ptr, char_ptr, bytes_to_copy);
        }

        return new_ptr;
    }
}

void* aligned_palloc(unsigned int size, uint8_t alignment)
{
    // TODO: No addresses we allocate will ever be 32-bit+ aligned
    // right now, because of the combination of prefix bytes + byte per slice
    if (alignment > 16) return NULL;

    return rust_aligned_alloc(g_allocator, size, alignment);
}

void* palloc(unsigned int size)
{
    return rust_alloc(g_allocator, size);
}

void* pcalloc(unsigned int item_size, unsigned int size)
{
    // Allocate the memory
    void* mem = rust_alloc(g_allocator, item_size * size);

    if(mem == NULL)
        return NULL;

    // Zero it out
    my_memset(mem, 0, item_size * size);

    return mem;
}

void phree(void* pointer)
{
    rust_free(g_allocator, pointer);
}

