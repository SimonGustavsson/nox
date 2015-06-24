#include <types.h>
#include <terminal.h>
#include <kernel.h>
#include <fat.h>

// -------------------------------------------------------------------------
// Static Declares
// -------------------------------------------------------------------------
#define FAT12_EOF 0x0FF8;
#define FAT16_EOF 0xFFF8
#define FAT32_EOF 0x0FFFFFF8

#define FAT12_BAD 0x0FF7;
#define FAT16_BAD 0xFFF7
#define FAT32_BAD 0x0FFFFFF7

// -------------------------------------------------------------------------
// External functions
// -------------------------------------------------------------------------
// This is supposed to be an extern function or included from a header  <Smirk />
// (arguments are compared to avoid "Unused variable"-warnings.
#define read_sector(x, y) (x == x && y == y)
// -------------------------------------------------------------------------
// Forward Declares
// -------------------------------------------------------------------------
uint32_t get_fat_entry_for_cluster(struct bpb* bpb, uint32_t cluster);
bool is_eof(struct bpb* bpb, uint32_t fat_entry);
bool is_bad(struct bpb* bpb, uint32_t fat_entry);

// -------------------------------------------------------------------------
// Public Contract
// -------------------------------------------------------------------------
uint32_t get_fat_entry_for_cluster(struct bpb* bpb, uint32_t cluster)
{
    uint32_t fat_offset;
    enum fat_version fat_type = fat_get_version(bpb);
    switch(fat_type) {
        case fat_version_12:
            fat_offset = cluster + (cluster / 2);
            KERROR("FAT 12 is not supported becuase it's dumb.");
            return 0;
        case fat_version_16:
            fat_offset = cluster * 2;
            break;
        case fat_version_32:
            fat_offset = cluster * 4;
            break;
    }

    // Perform this into a temp variable so that the division and remainder
    // happens right after eachother so the compiler can optimize it as a single MUL instruction
    uint32_t tmp = fat_offset / bpb->bytes_per_sector;
    uint32_t fat_entry_offset = fat_offset % bpb->bytes_per_sector;

    uint32_t fat_sector = bpb->reserved_sector_count + tmp;

    uint8_t fat_buffer[bpb->bytes_per_sector];

    if(!read_sector(fat_sector, &fat_buffer[0])) {
        KWARN("Failed to read FAT sector");
        return 0;
    }

    switch(fat_type) {
        case fat_version_12:
        {
            uint16_t entry_12 = *((uint16_t*)&fat_buffer[fat_entry_offset]);
            if(fat_entry_offset == (bpb->bytes_per_sector - 1)) {
                // This fat entry spans fat sectors, this means we need
                // to read in the next fat sector (unless it's the last)
                // Note: It's probably a better idea to always just read
                //       two fat sectors every time when dealing with FAT12
                uint8_t fat_buffer2[bpb->bytes_per_sector];

                // TODO: This needs a check to ensure fat_sector is not
                //       the very last fat sector, as we don't want to read past that
                if(!read_sector(fat_sector + 1, &fat_buffer2[0])) {
                    KWARN("Failed to read second sector of fat entry for fat12");
                    return 0;
                }
                // Set the low 8 bits of the fat entry from the new sector we read
                entry_12 =  (entry_12 & 0xF0) | *((uint8_t*)&fat_buffer2[0]);
            }

            // For EVEN clusters, we only want the low 12 bits
            // for ODD clusters, we only want the high 12 bits
            if(cluster & 0x0001)
                return entry_12 >> 4;
            else
                return entry_12 & 0x0FFF;
        }
        case fat_version_16:
            return *((uint16_t*) &fat_buffer[fat_entry_offset]);
        default:
            return (*((uint32_t*) &fat_buffer[fat_entry_offset])) & 0x0FFFFFFF;
    }
}

enum fat_version fat_get_version(struct bpb* bpb)
{
    uint32_t root_dir_sector_count = ((bpb->root_entry_count * 32) + (bpb->bytes_per_sector - 1)) / bpb->bytes_per_sector;

    uint32_t fat_size;
    if(bpb->fat_size16 != 0)
        fat_size = bpb->fat_size16;
    else
        fat_size = bpb->ebpb.ebpb32.fat_size32;
    
    uint32_t total_sectors;
    if(bpb->total_sectors16 != 0)
        total_sectors = bpb->total_sectors16;
    else
        total_sectors = bpb->total_sectors32;

    uint32_t data_sector = total_sectors - (bpb->reserved_sector_count + (bpb->num_fats * fat_size) + root_dir_sector_count);

    uint32_t cluster_count = data_sector / bpb->sectors_per_cluster;
    
    if(cluster_count < 4085) 
        return fat_version_12;
    else if(cluster_count < 65525)
        return fat_version_16;
    else
        return fat_version_32;
}

// -------------------------------------------------------------------------
// Static Utilities
// -------------------------------------------------------------------------
bool is_eof(struct bpb* bpb, uint32_t fat_entry)
{
    switch(fat_get_version(bpb)) {
        case fat_version_12:
            return fat_entry > FAT12_EOF;
        case fat_version_16:
            return fat_entry > FAT16_EOF;
        default:
            return fat_entry > FAT32_EOF;
    }
}

bool is_bad(struct bpb* bpb, uint32_t fat_entry)
{
    switch(fat_get_version(bpb)) {
        case fat_version_12:
            return fat_entry == FAT12_BAD;
        case fat_version_16:
            return fat_entry == FAT16_BAD;
        default:
            return fat_entry == FAT32_BAD;
    }
}

