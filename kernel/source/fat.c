#include <types.h>
#include <terminal.h>
#include <kernel.h>
#include <fs.h>
#include <fat.h>
#include <mem_mgr.h>
#include <ata.h>
#include <string.h>

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
static void dump_fat_part_info(struct fat_part_info* info);
static inline bool is_directory(uint8_t attribute);
static inline bool is_system(uint8_t attribute);
static inline bool is_volume_id(uint8_t attribute);
static void dump_fat_dir_entry(struct fat_dir_entry* entry);

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

    struct fat_dir_entry kernel_entry;

    if(!fat_get_dir_entry(info_result, "KERNEL  BIN", &kernel_entry)) {
        KWARN("Couldn't find kernel on file system. Name changed?");
    }
    else {
       dump_fat_dir_entry(&kernel_entry);
    }

    // Free the buffer, we don't need it no more
    mem_page_free(buffer);

    terminal_indentation_decrease();
    return true;
}

// Note: This is currently limited to the root director
//       In the future it could take in the fat_dir_entry of the directory to look in
bool fat_get_dir_entry(struct fat_part_info* part_info, const char* filename83, struct fat_dir_entry* result)
{
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

        if(!kstrcmp_n(entry->name, filename83, 11))
            continue; // Not this file!

        // Copy into result as the entry we have will be freed
        kstrcpy_n((char*)result, sizeof(struct fat_dir_entry), (char*)entry);

        mem_page_free((void*)buffer);
        return true;
    }

    mem_page_free((void*)buffer);
    return false;
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

static inline bool is_read_only(uint8_t attribute)
{
    return (attribute & fat_attr_read_only) == fat_attr_read_only;
}

static inline bool is_archive(uint8_t attribute)
{
    return (attribute & fat_attr_archive) == fat_attr_archive;
}

static inline bool is_hidden(uint8_t attribute)
{
    return (attribute & fat_attr_hidden) == fat_attr_hidden;
}

static inline bool is_lfn(uint8_t attribute)
{
   return is_read_only(attribute) &&
          is_system(attribute) &&
          is_hidden(attribute) &&
          is_volume_id(attribute); 
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

static void fat_time_to_string(uint16_t time, char result[8])
{
    // High 5-bits is hour
    uint8_t hour =(uint8_t) (time >> 11);
    uint8_t min = (uint8_t)((time >> 5) & 0x1F);
    uint8_t sec = (uint8_t)(time & 0x1F);

    result[0] = '0' + (hour / 10);
    result[1] = '0' + (hour % 10);
    result[2] = ':';
    result[3] = '0' + (min / 10);
    result[4] = '0' + (min % 10);
    result[5] = ':';
    result[6] = '0' + (sec / 10);
    result[7] = '0' + (sec % 10);
    result[8] = '\0';
}

static void fat_date_to_string(uint16_t date, char result[11])
{
//15-9	Year (0 = 1980, 119 = 2099 supported under DOS/Windows, theoretically up to 127 = 2107)
//8-5	Month (1–12)
//4-0	Day (1–31)
    uint16_t year = 1980 + ((uint8_t)(date >> 9));
    uint8_t month = (uint8_t)((date >> 5) & 0xF);
    uint8_t day  = (uint8_t)(date & 0x1F);

    itoa(year, result);
    result[4] = '-';
    result[5] = '0' + (month / 10);
    result[6] = '0' + (month % 10);
    result[7] = '-';
    result[8] = '0' + (day / 10);
    result[9] = '0' + (day % 10);
    result[10] = '\0';
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

static void dump_fat_dir_entry(struct fat_dir_entry* entry)
{
    terminal_write_string("Dumping file entry info\n");

    terminal_write_string("Name: ");
    terminal_write_string_n(entry->name, 11);
    terminal_write_char('\n');

    terminal_write_string("Read Only? ");
    terminal_write_string(is_read_only(entry->attribute) ? "Yes " : "No ");
    terminal_write_string("Hidden? ");
    terminal_write_string(is_hidden(entry->attribute) ? "Yes " : "No ");
    terminal_write_string("System? ");
    terminal_write_string(is_system(entry->attribute) ? "Yes " : "No ");
    terminal_write_string("Volume Id? ");
    terminal_write_string(is_volume_id(entry->attribute) ? "Yes " : "No ");
    terminal_write_string("Dir? ");
    terminal_write_string(is_directory(entry->attribute) ? "Yes " : "No ");
    terminal_write_string("Archive? ");
    terminal_write_string(is_archive(entry->attribute) ? "Yes" : "No");
    terminal_write_char('\n');

    char time_str[8];
    char date_str[11];
    terminal_write_string("Created: ");

    fat_date_to_string(entry->create_date, date_str);
    terminal_write_string(date_str);
    terminal_write_char(' ');

    fat_time_to_string(entry->create_time, time_str);
    terminal_write_string(time_str);
    terminal_write_char('\n');

    terminal_write_string("Last accessed: ");

    fat_date_to_string(entry->last_access_date, date_str);
    terminal_write_string(date_str);
    terminal_write_char(' ');

    fat_time_to_string(entry->last_access_time, time_str);
    terminal_write_string(time_str);
    terminal_write_char('\n');

    terminal_write_string("Last modified: ");

    fat_date_to_string(entry->last_modified_date, date_str);
    terminal_write_string(date_str);
    terminal_write_char(' ');

    fat_time_to_string(entry->last_modified_time, time_str);
    terminal_write_string(time_str);
    terminal_write_char('\n');

    SHOWVAL("First cluster: ", entry->first_cluster);
    terminal_write_string("Size: ");
    terminal_write_uint32(entry->size);
    terminal_write_string(" bytes\n");
}

