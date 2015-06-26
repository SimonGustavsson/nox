#include <types.h>
#include <terminal.h>
#include <kernel.h>
#include <fs.h>
#include <fat.h>
#include <mem_mgr.h>
#include <ata.h>

// -------------------------------------------------------------------------
// Static Declares
// -------------------------------------------------------------------------
#define FAT12_EOF 0x0FF8;
#define FAT16_EOF 0xFFF8
#define FAT32_EOF 0x0FFFFFF8

#define FAT12_BAD 0x0FF7;
#define FAT16_BAD 0xFFF7
#define FAT32_BAD 0x0FFFFFF7

#define ROOT_ENTRY_SIZE 32

//#define DEBUG_FAT

// -------------------------------------------------------------------------
// External functions
// -------------------------------------------------------------------------
// This is supposed to be an extern function or included from a header  <Smirk />
// (arguments are compared to avoid "Unused variable"-warnings.
#define read_sector(x, y) (x == x && y == y)
// -------------------------------------------------------------------------
// Forward Declares
// -------------------------------------------------------------------------
static uint32_t get_fat_entry_for_cluster(struct bpb* bpb, uint32_t cluster);
static bool is_eof(struct fat_part_info* part_info, uint32_t fat_entry);
static bool is_bad(struct fat_part_info* part_info, uint32_t fat_entry);
static const char* fat_version_to_string(enum fat_version version);
bool enumerate_root_dir(struct fat_part_info* part_info);
static void dump_fat_part_info(struct fat_part_info* info);
static inline bool is_directory(uint8_t attribute);
static inline bool is_system(uint8_t attribute);
static inline bool is_volume_id(uint8_t attribute);

// -------------------------------------------------------------------------
// Public Contract
// -------------------------------------------------------------------------
//enum fat_part_info {
//    struct mbr_partiton_entry mbr_entry;
//    uint32_t                  root_dir_sector;
//};
bool fat_init(struct mbr_partition_entry* partition_entry, struct fat_part_info* info_result)
{
    terminal_write_string("FAT: Initializing partition.\n");
    terminal_indentation_increase();

    uint8_t* buffer = (uint8_t*)(intptr_t)(mem_page_get());

    if(!ata_read_sectors(partition_entry->lba_begin, 1, (intptr_t)buffer)) {
        KWARN("Failed to read first sector of FAT partition");
        return false;
    }

    struct bpb* bpb = (struct bpb*)(intptr_t)(buffer);

    info_result->total_sectors = bpb->total_sectors16 != 0 ? bpb->total_sectors16 : bpb->total_sectors32;
    info_result->fat_begin = bpb->reserved_sector_count + bpb->hidden_sectors;
    info_result->fat_size = bpb->fat_size16 > 0 ? bpb->fat_size16 : bpb->ebpb.ebpb32.fat_size32;
    info_result->num_root_dir_sectors = ((bpb->root_entry_count * ROOT_ENTRY_SIZE) + (bpb->bytes_per_sector - 1)) / bpb->bytes_per_sector;
    info_result->data_begin = bpb->reserved_sector_count + (bpb->num_fats * info_result->fat_size) + info_result->num_root_dir_sectors;
    info_result->num_sectors_per_cluster = bpb->sectors_per_cluster;
    info_result->fat_total_sectors = (info_result->fat_size * bpb->num_fats); 
    info_result->root_dir_sector = info_result->fat_begin + info_result->fat_total_sectors;
    info_result->version = fat_get_version(info_result);
    info_result->bytes_per_sector = bpb->bytes_per_sector;

#ifdef DEBUG_FAT
    dump_fat_part_info(info_result);
#endif

    // Thanks for the -Wall, Philip...
    if(get_fat_entry_for_cluster(bpb, 0)) {
        is_eof(info_result, 42);
        is_bad(info_result, 42);
        dump_fat_part_info(info_result);
    }

    enumerate_root_dir(info_result);

    // Free the buffer, we don't need it no more
    mem_page_free(buffer);

    terminal_indentation_decrease();
    return true;
}

bool enumerate_root_dir(struct fat_part_info* part_info)
{
    KINFO("Enumerating root directory");

    if(part_info->bytes_per_sector > PAGE_SIZE) {
        KERROR("Sectors size too large for page allocation! NEed more pages!");
        return false;
    }

    if(part_info->num_sectors_per_cluster < part_info->num_root_dir_sectors) {
        KWARN("Root directory spans clusters, wont read all entries!");
    }

    intptr_t buffer = (intptr_t)mem_page_get_many(part_info->num_sectors_per_cluster);    
    if(!ata_read_sectors(part_info->root_dir_sector, 1, buffer)) {
        KWARN("Failed to read first sector of FAT partition");
        return false;
    }

    struct fat_dir_entry* root_entries = (struct fat_dir_entry*)(buffer);

    size_t entries_in_cluster = (part_info->num_sectors_per_cluster * part_info->bytes_per_sector) /
        sizeof(struct fat_dir_entry);

    for(int i = 0; i < entries_in_cluster; i++) {
        struct fat_dir_entry* entry = &root_entries[i];
        if(entry->name[0] == 0)
            break; // No more entries
        if(entry->name[0] == 0xE5)
            continue; // Unused entry

        if(is_volume_id(entry->attribute) || is_system(entry->attribute))
            continue; // Skip Volume Id etc

        terminal_write_string((char*)root_entries[i].name);
        terminal_write_char('\n');
    }

    mem_page_free((void*)buffer);
    return true;
}

static uint32_t get_fat_entry_for_cluster(struct bpb* bpb, uint32_t cluster)
{
    return 0;
/*
   // Untested code below - Serves more as a reference at this point...
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
    */
}

enum fat_version fat_get_version(struct fat_part_info* part_info)
{
    if(part_info->total_sectors < 4085) 
        return fat_version_12;
    else if(part_info->total_sectors < 65525)
        return fat_version_16;
    else
        return fat_version_32;
}

// -------------------------------------------------------------------------
// Static Utilities
// -------------------------------------------------------------------------
static inline bool is_volume_id(uint8_t attribute)
{
    return (attribute & fat_attr_volume_id) == fat_attr_volume_id;
}

static inline bool is_system(uint8_t attribute)
{
    return (attribute & fat_attr_system) == fat_attr_system;
}

static inline bool is_directory(uint8_t attribute)
{
    return (attribute & fat_attr_dir) == fat_attr_dir;
}

static bool is_eof(struct fat_part_info* part_info, uint32_t fat_entry)
{
    switch(part_info->version) {
        case fat_version_12:
            return fat_entry > FAT12_EOF;
        case fat_version_16:
            return fat_entry > FAT16_EOF;
        default:
            return fat_entry > FAT32_EOF;
    }
}

static bool is_bad(struct fat_part_info* part_info, uint32_t fat_entry)
{
    switch(part_info->version) {
        case fat_version_12:
            return fat_entry == FAT12_BAD;
        case fat_version_16:
            return fat_entry == FAT16_BAD;
        default:
            return fat_entry == FAT32_BAD;
    }
}

static const char* fat_version_to_string(enum fat_version version)
{
    switch(version) {
        case fat_version_12: return "FAT12";
        case fat_version_16: return "FAT16";
        case fat_version_32: return "FAT32";
        default: return "Unknown";
    };
}

static void dump_fat_part_info(struct fat_part_info* info)
{
    SHOWVAL("Root dir sector: ", info->root_dir_sector);
    SHOWVAL("Root dir size: ", info->num_root_dir_sectors);
    SHOWVAL("Data begin: ", info->data_begin);
    SHOWVAL("Sectors per cluster: ", info->num_sectors_per_cluster);
    SHOWVAL("FAT begin: ", info->fat_begin);
    SHOWVAL("FAT size: ", info->fat_size);
    SHOWVAL("FAT total sectors: ", info->fat_total_sectors);
    SHOWVAL("Total sectors: ", info->total_sectors);
    SHOWVAL("Bytes per sector: ", info->bytes_per_sector);
    SHOWSTR("Version: ", fat_version_to_string(info->version));
}

