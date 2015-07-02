#include <types.h>
#include <kernel.h>
#include <fs.h>
#include <fat.h>
#include <terminal.h>
#include <mem_mgr.h>

#define EXE_MAX_SEGMENTS 200

enum elf32_class {
    elf32_class_invalid = 0,
    elf32_class_32 = 1,
    elf32_class_64 = 2
};

enum elf32_encoding {
    elf32_encoding_invalid = 0,
    elf32_encoding_lsb = 1,
    elf32_encoding_msb = 2
};

enum elf32_version {
    elf32_version_invalid = 0,
    elf32_version_current = 1
};

struct elf32_header_ident {
    char signature[4];
    uint8_t class;
    uint8_t encoding;
    uint8_t version;
    uint8_t padding[9];
} PACKED;

enum elf32_machine {
    elf32_machine_none  = 0,   // No Machine
    elf32_machine_m32   = 1,   // AT&T WE 32100
    elf32_machine_sparc = 2,   // SPARC
    elf32_machine_386   = 3,   // Intel 80386
    elf32_machine_68k   = 4,   // Motorola 68000
    elf32_machine_88k   = 5,   // Motorola 88000
    // Note: No 6!
    elf32_machine_860   = 7,   // Intel 80860
    elf32_machine_mips  = 8    // MIPS RS3000
};

struct elf32_header {
    struct elf32_header_ident ident; 
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} PACKED;

enum sh_type {
    sh_type_null = 0,
    sh_type_progbits = 1,
    sh_type_symtab = 2,
    sh_type_strtab = 3,
    sh_type_rela = 4,
    sh_type_hash = 5,
    sh_type_dynamic = 6,
    sh_type_note = 7,
    sh_type_nobits = 8,
    sh_type_rel = 9,
    sh_type_shlib = 10,
    sh_type_dynsym = 11,
    sh_type_loproc = 0x70000000,
    sh_type_hiproc = 0x7fffffff,
    sh_type_louser = 0x80000000,
    sh_type_hiuser = 0xffffffff
};

enum elf32_sh_flag {
    elf32_sh_flag_write = 1,
    elf32_sh_flag_alloc = 2,
    elf32_sh_flag_exec = 4
};

struct elf32_shdr {
    uint32_t name;
    uint32_t type;
    uint32_t flags; // See elf32_sh_flag
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} PACKED;

enum ph_type {
    ph_type_null = 0,
    ph_type_load = 1,
    ph_type_dynamic = 2,
    ph_type_interp = 3,
    ph_type_note = 4,
    ph_type_shlib = 5,
    ph_type_phdr = 6,
    ph_type_loproc = 0x70000000,
    ph_type_hiproc = 0x7fffffff
};
// Flags	Value	Exact	Allowable
// none	0	All access denied	All access denied
// PF_X	1	Execute only	Read, execute
// PF_W	2	Write only	Read, write, execute
// PF_W+PF_X	3	Write, execute	Read, write, execute
// PF_R	4	Read only	Read, execute
// PF_R+PF_X	5	Read, execute	Read, execute
// PF_R+PF_W	6	Read, write	Read, write, execute
// PF_R+PF_W+PF_X	7	Read, write, execute	Read, write, execute
enum elf32_ph_flag {
    elf32_ph_flag_none = 0,
    elf32_ph_flag_x = 1,
    elf32_ph_flag_w = 2,
    elf32_ph_flag_r = 4
};

struct elf32_phdr {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t file_size;
    uint32_t mem_size;
    uint32_t flags; // see elf32_ph_flag 
    uint32_t align;
} PACKED;

static void print_sh_type(enum sh_type type, size_t total_len)
{
    switch(type) {
        case sh_type_null:
            terminal_write_string_endpadded("NULL", total_len);
            break;
        case sh_type_progbits:
            terminal_write_string_endpadded("PROGBITS", total_len);
            break;
        case sh_type_strtab:
            terminal_write_string_endpadded("STRTAB", total_len);
            break;
        case sh_type_symtab:
            terminal_write_string_endpadded("SYMTAB", total_len);
            break;
        case sh_type_nobits:
            terminal_write_string_endpadded("NOBITS", total_len);
            break;
        default:
            terminal_write_string_endpadded("Unknown!", total_len);
            break;
    }
}

static void print_ph_type(enum ph_type type, size_t total_len)
{
    switch(type) {
        case ph_type_null:
            terminal_write_string_endpadded("NULL", total_len);
            break;
        case ph_type_load:
            terminal_write_string_endpadded("LOAD", total_len);
            break;
        case ph_type_dynamic:
            terminal_write_string_endpadded("DYNAMIC", total_len);
            break;
        case ph_type_interp:
            terminal_write_string_endpadded("INTERP", total_len);
            break;
        case ph_type_note:
            terminal_write_string_endpadded("NOTE", total_len);
            break;
        case ph_type_shlib:
            terminal_write_string_endpadded("SHLIB", total_len);
            break;
        case ph_type_phdr:
            terminal_write_string_endpadded("PHDR", total_len);
            break;
        default:
            terminal_write_string_endpadded("INVALID", total_len);
            break;
    }
}

static void print_ph_flags(uint32_t flags)
{
    if((flags & elf32_ph_flag_r) == elf32_ph_flag_r)
        terminal_write_char('R');
    else
        terminal_write_char(' ');

    if((flags & elf32_ph_flag_w) == elf32_ph_flag_w)
        terminal_write_char('W');
    else
        terminal_write_char(' ');

    if((flags & elf32_ph_flag_x) == elf32_ph_flag_x)
        terminal_write_char('X');
    else
        terminal_write_char(' ');
}

static void print_sh_flags(uint32_t flags)
{
    if((flags & elf32_sh_flag_alloc) == elf32_sh_flag_alloc)
        terminal_write_char('A');
    else
        terminal_write_char(' ');

    if((flags & elf32_sh_flag_write) == elf32_sh_flag_write)
        terminal_write_char('W');
    else
        terminal_write_char(' ');

    if((flags & elf32_sh_flag_exec) == elf32_sh_flag_exec)
        terminal_write_char('X');
    else
        terminal_write_char(' ');
}

static void print_section_headers(struct elf32_header* elf, intptr_t buffer)
{
    terminal_write_string("Section headers:\n");
    terminal_write_string("[Nr] Name               Type           Addr       Offset   Size     ES   Flg\n");

    struct elf32_shdr* section_headers = (struct elf32_shdr*)(intptr_t)(((char*)buffer) + elf->shoff);

    struct elf32_shdr* sh_string_table = &section_headers[elf->shstrndx];
    char* str_table_start = ((char*)buffer) + sh_string_table->offset;

    for(uint8_t i = 0; i < elf->shnum; i++) {
        struct elf32_shdr* cur = &section_headers[i];

        terminal_write_char('[');
        uint8_t first_nr = i / 10;
        terminal_write_char(first_nr == 0 ? ' ' : '0' + first_nr);
        terminal_write_char('0' + (i % 10));
        terminal_write_char(']');
        terminal_write_char(' ');

        char* name = (str_table_start + cur->name);
        terminal_write_string_endpadded(name, 18);
        terminal_write_char(' ');
        print_sh_type(cur->type, 15);
        terminal_write_uint32_x(cur->addr);
        terminal_write_char(' ');
        terminal_write_uint24_x(cur->offset);
        terminal_write_char(' ');
        terminal_write_uint24_x(cur->size);
        terminal_write_char(' ');
        terminal_write_uint8_x((uint8_t)(cur->entsize));
        terminal_write_char(' ');
        print_sh_flags(cur->flags);
        terminal_write_string("\n");
    }
}

static void print_program_headers(struct elf32_header* elf, intptr_t buffer)
{
    struct elf32_phdr* program_headers = (struct elf32_phdr*)(intptr_t)(((char*)buffer) + elf->phoff);
    KINFO("Program Headers:");

    // Format:
    // Type           Offset   VirtAddr   PhysAddr   FileSiz  Flg Align
    // PHDR           0x000000 0x00000000 0x00000000 0x000000 RWE 0x4
    terminal_write_string("Type           Offset   VirtAddr   PhysAddr   FileSiz  Flg Align\n");
    for(size_t i = 0; i < elf->phnum; i++) {
        struct elf32_phdr* cur = &program_headers[i];

        print_ph_type(cur->type, 14);
        terminal_write_char(' ');
        terminal_write_uint24_x(cur->offset);
        terminal_write_char(' ');
        terminal_write_uint32_x(cur->vaddr);
        terminal_write_char(' ');
        terminal_write_uint32_x(cur->paddr);
        terminal_write_char(' ');
        terminal_write_uint24_x(cur->file_size);
        terminal_write_char(' ');
        print_ph_flags (cur->flags);
        terminal_write_char(' ');
        terminal_write_uint8_x(cur->align);
        terminal_write_string("\n");
    }
}

static bool verify_header(struct elf32_header* header)
{
    if(header->ehsize != sizeof(struct elf32_header)) {
        KWARN("Unexpected size of elf header\n");
    }

    if(header->ident.signature[0] != 0x7F ||
       header->ident.signature[1] != 'E' ||
       header->ident.signature[2] != 'L' ||
       header->ident.signature[3] != 'F') {
        KERROR("Invalid ELF header");
        return false;
    }

    if (header->ident.version != elf32_version_current) {
        KERROR("Invalid version");
        return false;
    }

    if(header->phnum > EXE_MAX_SEGMENTS) {
        KERROR("Too many program headers");
        return false;
    }

    if(header->machine != elf32_machine_386) {
        KERROR("Unsupported machine, expected 386");
        return false;
    }

    if(header->ident.class == elf32_class_invalid) {
        KERROR("Invalid class");
        return false;
    }

    // All checks passed! Looks good!
    return true;
}

void elf_run(const char* filename) 
{
    struct fat_dir_entry entry;
    if(!fat_get_dir_entry(fs_get_system_part(), filename, &entry)) {
        KERROR("That file doesn't exist!");
        return;
    }

    size_t pages_req = entry.size / PAGE_SIZE;
    if(entry.size % PAGE_SIZE)
        pages_req++;

    intptr_t buffer = (intptr_t)mem_page_get_many(pages_req);

    if(!fat_read_file(fs_get_system_part(), &entry, buffer, pages_req * PAGE_SIZE)) {
        KERROR("Failed to read file");
        return;
    }

    // We have a buffer, yay!
    struct elf32_header* elf = (struct elf32_header*)buffer;

    if(!verify_header(elf)) {
        return;
    }

    terminal_write_string("Running elf '");
    terminal_write_string(filename);
    terminal_write_string("'\n");

    switch (elf->ident.class) {
        case elf32_class_32:
            terminal_write_string("32-bit ELF.\n");
            break;

        case elf32_class_64:
            terminal_write_string("64-bit ELF.\n");
            break;
    }

    if(elf->phoff == 0) {
        KINFO("Elf has no program headers.");
        return;
    }

    print_section_headers(elf, buffer);

    print_program_headers(elf, buffer);
}

