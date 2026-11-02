#include "loader/elf_image_check.h"
#include "loader/elf.h"
#include <stdio.h>
#include <string.h>

/*
 * The checks below read ELF32 and ELF64 structures into these class-neutral
 * views, so that every rule is written once.
 */
typedef struct {
	std_bool is_64;
	uint64 phoff;
	uint64 shoff;
	uint32 phentsize;
	uint32 phnum;
	uint32 shentsize;
	uint32 shnum;
	uint32 shstrndx;
} ElfHeaderView;

typedef struct {
	uint32 type;
	uint64 offset;
	uint64 filesz;
	uint64 memsz;
} ElfProgramView;

typedef struct {
	uint32 name;
	uint32 type;
	uint64 offset;
	uint64 size;
	uint32 link;
	uint64 entsize;
} ElfSectionView;

static std_bool range_is_inside(uint64 offset, uint64 size, uint64 elf_size)
{
	if (offset > elf_size) {
		return FALSE;
	}
	return size <= (elf_size - offset);
}

/* count * entry_size bytes at offset, without overflowing the product. */
static std_bool table_is_inside(uint64 offset, uint32 count, uint32 entry_size, uint64 elf_size)
{
	if (count == 0U) {
		return TRUE;
	}
	if ((uint64)entry_size > (elf_size / count)) {
		return FALSE;
	}
	return range_is_inside(offset, (uint64)count * entry_size, elf_size);
}

static Std_ReturnType invalid(const char *reason, uint64 value)
{
	printf("Invalid ELF file: %s (0x%llx)\n", reason, (unsigned long long)value);
	return STD_E_INVALID;
}

static Std_ReturnType read_header(const uint8 *elf_data, uint64 elf_size, ElfHeaderView *header)
{
	if (elf_size < EI_NIDENT) {
		return invalid("file is shorter than the ELF identification", elf_size);
	}
	if (IS_ELF(*(const Elf32_Ehdr *)elf_data) == FALSE) {
		return invalid("no ELF magic number", 0U);
	}
	if (elf_data[EI_DATA] != ELFDATA2LSB) {
		return invalid("not little endian", elf_data[EI_DATA]);
	}
	if (elf_data[EI_CLASS] == ELFCLASS32) {
		Elf32_Ehdr ehdr;
		if (elf_size < sizeof(ehdr)) {
			return invalid("file is shorter than the ELF32 header", elf_size);
		}
		memcpy(&ehdr, elf_data, sizeof(ehdr));
		header->is_64 = FALSE;
		header->phoff = ehdr.e_phoff;
		header->shoff = ehdr.e_shoff;
		header->phentsize = ehdr.e_phentsize;
		header->phnum = ehdr.e_phnum;
		header->shentsize = ehdr.e_shentsize;
		header->shnum = ehdr.e_shnum;
		header->shstrndx = ehdr.e_shstrndx;
		return STD_E_OK;
	}
	if (elf_data[EI_CLASS] == ELFCLASS64) {
		Elf64_Ehdr ehdr;
		if (elf_size < sizeof(ehdr)) {
			return invalid("file is shorter than the ELF64 header", elf_size);
		}
		memcpy(&ehdr, elf_data, sizeof(ehdr));
		header->is_64 = TRUE;
		header->phoff = ehdr.e_phoff;
		header->shoff = ehdr.e_shoff;
		header->phentsize = ehdr.e_phentsize;
		header->phnum = ehdr.e_phnum;
		header->shentsize = ehdr.e_shentsize;
		header->shnum = ehdr.e_shnum;
		header->shstrndx = ehdr.e_shstrndx;
		return STD_E_OK;
	}
	return invalid("unknown ELF class", elf_data[EI_CLASS]);
}

static void read_program(const uint8 *elf_data, const ElfHeaderView *header, uint32 index, ElfProgramView *program)
{
	const uint8 *entry = elf_data + header->phoff + (uint64)index * header->phentsize;

	if (header->is_64) {
		Elf64_Phdr phdr;
		memcpy(&phdr, entry, sizeof(phdr));
		program->type = phdr.p_type;
		program->offset = phdr.p_offset;
		program->filesz = phdr.p_filesz;
		program->memsz = phdr.p_memsz;
	}
	else {
		Elf32_Phdr phdr;
		memcpy(&phdr, entry, sizeof(phdr));
		program->type = phdr.p_type;
		program->offset = phdr.p_offset;
		program->filesz = phdr.p_filesz;
		program->memsz = phdr.p_memsz;
	}
}

static void read_section(const uint8 *elf_data, const ElfHeaderView *header, uint32 index, ElfSectionView *section)
{
	const uint8 *entry = elf_data + header->shoff + (uint64)index * header->shentsize;

	if (header->is_64) {
		Elf64_Shdr shdr;
		memcpy(&shdr, entry, sizeof(shdr));
		section->name = shdr.sh_name;
		section->type = shdr.sh_type;
		section->offset = shdr.sh_offset;
		section->size = shdr.sh_size;
		section->link = shdr.sh_link;
		section->entsize = shdr.sh_entsize;
	}
	else {
		Elf32_Shdr shdr;
		memcpy(&shdr, entry, sizeof(shdr));
		section->name = shdr.sh_name;
		section->type = shdr.sh_type;
		section->offset = shdr.sh_offset;
		section->size = shdr.sh_size;
		section->link = shdr.sh_link;
		section->entsize = shdr.sh_entsize;
	}
}

static uint32 read_symbol_name(const uint8 *elf_data, const ElfHeaderView *header, uint64 offset)
{
	if (header->is_64) {
		Elf64_Sym sym;
		memcpy(&sym, elf_data + offset, sizeof(sym));
		return sym.st_name;
	}
	else {
		Elf32_Sym sym;
		memcpy(&sym, elf_data + offset, sizeof(sym));
		return sym.st_name;
	}
}

static Std_ReturnType check_programs(const uint8 *elf_data, uint64 elf_size, const ElfHeaderView *header)
{
	uint32 entry_size = header->is_64 ? sizeof(Elf64_Phdr) : sizeof(Elf32_Phdr);
	ElfProgramView program;
	uint32 i;

	if (header->phnum == 0U) {
		return STD_E_OK;
	}
	if (header->phentsize < entry_size) {
		return invalid("program header entry size too small", header->phentsize);
	}
	if (table_is_inside(header->phoff, header->phnum, header->phentsize, elf_size) == FALSE) {
		return invalid("program header table outside the file", header->phoff);
	}
	for (i = 0; i < header->phnum; i++) {
		read_program(elf_data, header, i, &program);
		if (program.type != PT_LOAD) {
			continue;
		}
		if (range_is_inside(program.offset, program.filesz, elf_size) == FALSE) {
			return invalid("PT_LOAD contents outside the file", program.offset);
		}
		if (program.filesz > program.memsz) {
			return invalid("PT_LOAD file size larger than memory size", program.filesz);
		}
	}
	return STD_E_OK;
}

/* A string table must be inside the file and end with NUL. */
static std_bool string_table_is_valid(const uint8 *elf_data, const ElfSectionView *section)
{
	if (section->type != SHT_STRTAB) {
		return FALSE;
	}
	if (section->size == 0U) {
		return FALSE;
	}
	return elf_data[section->offset + section->size - 1U] == '\0';
}

static Std_ReturnType check_symbol_table(const uint8 *elf_data, const ElfHeaderView *header, const ElfSectionView *symtab)
{
	uint32 entry_size = header->is_64 ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);
	ElfSectionView strtab;
	uint64 count;
	uint64 i;

	if ((symtab->entsize != 0U) && (symtab->entsize != entry_size)) {
		return invalid("symbol table entry size", symtab->entsize);
	}
	if (symtab->link >= header->shnum) {
		return invalid("symbol table string table index", symtab->link);
	}
	read_section(elf_data, header, symtab->link, &strtab);
	if (string_table_is_valid(elf_data, &strtab) == FALSE) {
		return invalid("symbol string table is not a NUL-terminated string table", symtab->link);
	}
	count = symtab->size / entry_size;
	for (i = 0; i < count; i++) {
		uint32 name = read_symbol_name(elf_data, header, symtab->offset + i * entry_size);
		if (name >= strtab.size) {
			return invalid("symbol name outside the string table", name);
		}
	}
	return STD_E_OK;
}

static Std_ReturnType check_sections(const uint8 *elf_data, uint64 elf_size, const ElfHeaderView *header)
{
	uint32 entry_size = header->is_64 ? sizeof(Elf64_Shdr) : sizeof(Elf32_Shdr);
	ElfSectionView names;
	ElfSectionView section;
	uint32 i;

	if (header->shnum == 0U) {
		return STD_E_OK;
	}
	if (header->shentsize < entry_size) {
		return invalid("section header entry size too small", header->shentsize);
	}
	if (table_is_inside(header->shoff, header->shnum, header->shentsize, elf_size) == FALSE) {
		return invalid("section header table outside the file", header->shoff);
	}
	for (i = 0; i < header->shnum; i++) {
		read_section(elf_data, header, i, &section);
		if ((section.type == SHT_NULL) || (section.type == SHT_NOBITS)) {
			continue;
		}
		if (range_is_inside(section.offset, section.size, elf_size) == FALSE) {
			return invalid("section contents outside the file", i);
		}
	}
	if (header->shstrndx >= header->shnum) {
		return invalid("section name table index", header->shstrndx);
	}
	read_section(elf_data, header, header->shstrndx, &names);
	if (string_table_is_valid(elf_data, &names) == FALSE) {
		return invalid("section name table is not a NUL-terminated string table", header->shstrndx);
	}
	for (i = 0; i < header->shnum; i++) {
		read_section(elf_data, header, i, &section);
		if (section.name >= names.size) {
			return invalid("section name outside the section name table", i);
		}
		if (section.type == SHT_SYMTAB) {
			if (check_symbol_table(elf_data, header, &section) != STD_E_OK) {
				return STD_E_INVALID;
			}
		}
	}
	return STD_E_OK;
}

Std_ReturnType elf_image_check(const uint8 *elf_data, uint64 elf_size)
{
	ElfHeaderView header;

	if (read_header(elf_data, elf_size, &header) != STD_E_OK) {
		return STD_E_INVALID;
	}
	if (check_programs(elf_data, elf_size, &header) != STD_E_OK) {
		return STD_E_INVALID;
	}
	if (check_sections(elf_data, elf_size, &header) != STD_E_OK) {
		return STD_E_INVALID;
	}
	return STD_E_OK;
}
