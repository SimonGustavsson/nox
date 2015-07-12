#ifndef NOX_ELF_H
#define NOX_ELF_H

void elf_run(const char* filename);
bool elf_load_trusted(const char* filename, intptr_t* res_entry);
void elf_info(const char* filename);

#endif
