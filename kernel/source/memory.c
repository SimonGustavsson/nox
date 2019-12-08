#include "memory.h"
#include "util.h"
#include "types.h"
#include "mem_mgr.h"
#include "terminal.h"

// #define DEBUG_MEM
#define DEFAULT_ALLOC_ALIGNMENT (2)

unsigned char* g_bitmap;
unsigned char* g_memory;

unsigned int g_bytes_allocated;

void memory_init()
{
    g_bytes_allocated = 0;

    // TODO: Remove hardcoded max-memory limit
    uint32_t pages_to_allocate = MAX_ALLOCATED_BYTES / PAGE_SIZE;

    // 1 page for the bitmap
    pages_to_allocate += 1;

    // Allocate all of the pages we can, and store the bitmap in the first page
    g_bitmap = (unsigned char*) mem_page_get_many(pages_to_allocate);

    // ... and the rest of the pages is free for allocation!
    uint32_t bitmap_size = (MAX_ALLOCATED_SLICES / 8); // 8 = bits per byte
    g_memory = (unsigned char*)((g_bitmap) + bitmap_size);

    // Ensure memory map is clear (note that allocated memory might be dirty)
    my_memset((void*) g_bitmap, 0, bitmap_size);
}

static void mark_slices(unsigned int start, unsigned int count, bool used)
{
#ifdef DEBUG_MEM
    printf("Marking slices between %d and %d as %d\n\r", start, start + count, used);
#endif

    // Find bit to set it
    unsigned int start_byte = start / 8;
    unsigned int start_remainder = start % 8;

    unsigned int last_byte = (start + count) / 8;
    unsigned int last_remainder = (start + count) % 8;

    // Now to do some checking for cases
    if (start_byte == last_byte)
    {
        // Start and end is in the same byte
        // Example: Start: 0, count = 7, end = 7
        //	| 1 1 1 1 1 1 1 0 | 0 0 0 0 0 0	0 0 |
        //	| _____Byte0_____ | _____Byte1_____ |
        unsigned int bits_to_set_as_bits = (1 << count) - 1; // Bit magic: 127

        if (used)
            g_bitmap[start_byte] |= bits_to_set_as_bits << (8 - start_remainder - count);
        else
            g_bitmap[start_byte] &= ~(((char)bits_to_set_as_bits << (8 - start_remainder - count)));
    }
    else if (last_byte == start_byte + 1 && start_remainder == 0 && last_remainder == 0)
    {
        // The block of slices fills exactly an entire byte
        g_bitmap[start_byte] = used ? 0xFF : 0;
    }
    else
    {
        // Start and end is in different bytes
        unsigned int desired_start_pos = start_remainder;

        // 1. Set start
        // We know end is in a different byte, so we fill start byte, starting at desired pos
        unsigned int start_bits_to_set = 8 - desired_start_pos;
        unsigned int bits_to_set_as_bits = ((1 << start_bits_to_set) - 1);
        if (used)
            g_bitmap[start_byte] |= bits_to_set_as_bits;
        else
            g_bitmap[start_byte] &= ~bits_to_set_as_bits;

        unsigned int bits_left_to_set = count - start_bits_to_set;

        // 2. Set end (if not full)
        if (last_remainder > 0)
        {
            // We're only setting a specific amount of bits in the last byte
            unsigned int desired_end_pos = last_remainder;
            unsigned int bits_in_last_byte_to_set = last_remainder;
            bits_to_set_as_bits = ((1 << bits_in_last_byte_to_set) - 1);

            if (used)
                g_bitmap[last_byte] |= bits_to_set_as_bits << (8 - desired_end_pos);
            else
                g_bitmap[last_byte] &= ~(bits_to_set_as_bits << (8 - desired_end_pos));

            bits_left_to_set -= bits_in_last_byte_to_set;
        }

        // 3. Set all bytes inbetween
        if (bits_left_to_set > 0)
        {
            unsigned int num_bytes_spanned = bits_left_to_set / 8;
            unsigned int i;

#ifdef DEBUG_MEM
            printf("Setting %d bits between start and end.\n", num_bytes_spanned * 8);
#endif
            // start byte has already been set, and last byte has been set if it doesn't fill entire byte
            unsigned int last_byte_to_set = start_byte + 1 + num_bytes_spanned;

#ifdef DEBUG_MEM
            printf("I'm asked to set %d bytes as used. first = %d, last = %d\n",
                last_byte_to_set - (start_byte + 1),
                start_byte + 1, last_byte_to_set - 1);
#endif

            for (i = start_byte + 1; i < last_byte_to_set; i++)
                g_bitmap[i] = used ? 0xFF : 0;
        }
    }
}

static bool is_slice_aligned(uint32_t slice, uint8_t alignment)
{
    uint32_t addr = (uint32_t) (g_memory + (slice * BYTES_PER_SLICE));

    // We will prepend 4 bytes to the address before it is returned
    // Make sure that the *final* pointer is aligned
    addr += 4;

    return (addr % alignment) == 0;
}

static int get_first_available_slice(unsigned int requested_size, uint8_t alignment)
{
    unsigned int i, j;
    int clear_bits_start = -1;
    int clear_bits_found = 0;

#ifdef DEBUG_MEM
    printf("Searching for first block of %d available slices. Max allocated: %d\n", requested_size, MAX_ALLOCATED_SLICES);
#endif

    bool found_bits = 0;
    for (i = 0; i < MAX_ALLOCATED_SLICES / sizeof(char); i++)
    {
        //printf("CHecking byte %d, value: %d\n", i, g_bitmap[i]);
        if ((g_bitmap[i] & 0xFF) == 0xFF) { // No free in this byte, skip
            clear_bits_start = -1;
            clear_bits_found = 0;
        }
        else if (g_bitmap[i] == 0 && is_slice_aligned(i * sizeof(char) * 8, alignment)) {
            uint32_t start_candidate = i * (sizeof(char) * 8);
            if (clear_bits_start == -1) {
                if (is_slice_aligned(start_candidate, alignment)) {
                    clear_bits_start = start_candidate; // Found a new start
                    clear_bits_found = sizeof(char) * 8;
                } else {
                    // Go fish
                }
            }
            else {
                clear_bits_found += sizeof(char) * 8; // Add to the pile
            }
        }
        else {
            // OK - well that sucked, just loop and see how many clear bits we can find
            for (j = sizeof(char)* 8 - 1; j > 0; j--) {
                if ((g_bitmap[i] & (1 << j)) == 0) {
                    // j contains the shifted offset from the right, set start to be the 0 based index from the left
                    uint32_t start_candidate = ((i * (sizeof(char)* 8)) + (8 - j)) - 1;
                    if (clear_bits_start == -1) {
                        if (is_slice_aligned(start_candidate, alignment)) {
                            clear_bits_start = start_candidate;
                        } else {
                            continue; // Go fish
                        }
                    }

                    clear_bits_found += 1;

                    if (clear_bits_found >= requested_size) {
                        found_bits = true;
                        break; // Found a free block
                    }
                }
                else {
                    clear_bits_start = -1;
                    clear_bits_found = 0;
                }
            }
        }

        // Did we find a free block?
        if (clear_bits_start >= 0 && (unsigned int)clear_bits_found >= requested_size)
        {
            found_bits = true;
            break;
        }
    }

    if (!found_bits) {
#ifdef DEBUG_MEM
        printf("Could not find enough free memory\n");
#endif
        return -1;
    }

    if (clear_bits_start == -1) {
#ifdef DEBUG_MEM
        printf("Invalid slice start\n");
#endif
        return -1;
    }

    if (clear_bits_found < requested_size)
    {
#ifdef DEBUG_MEM
        printf("Couldn't locate enough slices\n");
#endif
        return -1;
    }

    return clear_bits_start;
}

void* realloc(void* ptr, unsigned int size)
{
    if (size == 0)
    {
        if (ptr != NULL)
        {
            phree(ptr);
        }

        return NULL;
    }
    else
    {
        void* new_ptr = palloc(size);
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

static void* palloc_core(unsigned int size, uint8_t alignment)
{
#ifdef DEBUG_MEM
    printf("Palloc(%d) (%d/%d)\n", size, g_bytes_allocated, MAX_ALLOCATED_BYTES);
#endif

    if (alignment % 2 != 0) {
        KERROR("AN attempt was made to use palloc() with an alignment that isn't a multiple of 2");
        return NULL;
    }

    int start_slice = -1;

    // Calculate number of slices necessary to store data
    unsigned int slice_count = (size / BYTES_PER_SLICE) + 1; // 1 slice for size

    if (size % BYTES_PER_SLICE)
        slice_count++;

    // Do we need an extended size byte?
    if (slice_count > 2147483647)// (Uint32 max value) - Invalid allocation size
        return NULL; // TODO: Support larger allocations?

    // Now find slice_count clear consecutive bits in the bitmap
    start_slice = get_first_available_slice(slice_count, alignment);

    // Did we manage to locate a free block of slices?
    if (start_slice == -1) {
        printf("PALLOC FAILED. Allocated memory: %d bytes\n", g_bytes_allocated);
        return NULL; // BOO! Could not find a slice large enough
    }

    // Calculate the address to the memory, and offset the size bytes
    unsigned char* ptr = g_memory + (start_slice * BYTES_PER_SLICE);

    // Write size bytes
    *ptr++ = ((slice_count >> 24) & 0xFF);
    *ptr++ = ((slice_count >> 16) & 0xFF);
    *ptr++ = ((slice_count >> 8) & 0xFF);
    *ptr++ = (slice_count & 0xFF);

    //Mark the slices as used in the bitmap
    mark_slices(start_slice, slice_count, true);

    g_bytes_allocated += size + 1; // 1 for size bytes

#ifdef DEBUG_MEM
    printf("Allocated %d bytes at 0x%h. %d left.\n", size + 1, ptr, MAX_ALLOCATED_BYTES - g_bytes_allocated);
#endif

    return ptr;
}

void* aligned_palloc(unsigned int size, uint8_t alignment)
{
    // TODO: No addresses we allocate will ever be 32-bit+ aligned
    // right now, because of the combination of prefix bytes + byte per slice
    if (alignment > 16) return NULL;

    return palloc_core(size, alignment);
}

void* palloc(unsigned int size)
{
    return palloc_core(size, DEFAULT_ALLOC_ALIGNMENT);
}

void* pcalloc(unsigned int item_size, unsigned int size)
{
    // Allocate the memory
    void* mem = palloc(item_size * size);

    if(mem == NULL)
        return NULL;

    // Zero it out
    my_memset(mem, 0, item_size * size);

    return mem;
}

void phree(void* pointer)
{
    // Free da pointah
    unsigned char* ptr = (unsigned char*)pointer;
    int num_slices = -1;

#ifdef DEBUG_MEM
    printf("phree(ptr: %d)\n", ptr);
#endif

    // Get slice count (Stored in 4 bytes preceeding the pointer)
    num_slices = (*(ptr - 4) << 24) |
        (*(ptr - 3) << 16) |
        (*(ptr - 2) << 8) |
        *(ptr - 1);

    assert3(num_slices > 0 && num_slices < MAX_ALLOCATED_SLICES, "phree(0x%h): Invalid arg, slice count: %d\n", ptr, num_slices);

    ptr -= 4; // Make sure we deallocate the size bytes as well

    // ptr now points to the address, figure out the offset to find the slice start number
    unsigned int start_slice = (ptr - g_memory) / BYTES_PER_SLICE;

    assert3(start_slice >= 0 && start_slice < MAX_ALLOCATED_SLICES - 2, "phree(0x%h): Invalid arg, slice start: %d\n", ptr, start_slice);

    //assert_uart(start_slice < 0 || start_slice > MAX_ALLOCATED_SLICES - 2, "Invalid ptr passed to phree()");

    mark_slices(start_slice, num_slices, false);

    g_bytes_allocated -= num_slices * 4;

#ifdef DEBUG_MEM
    printf("Freed %d bytes, %d allocated.\n", num_slices * 4, g_bytes_allocated);
#endif
}

