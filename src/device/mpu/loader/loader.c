#include "loader/loader.h"
#include "cpu.h"
#include "mpu.h"
#include "mpu_ops.h"
#include "loader/elf.h"
#include "elf_section.h"
#include "elf_dwarf_line.h"
#include "elf_dwarf_abbrev.h"
#include "elf_dwarf_info.h"
#include "elf_dwarf_loc.h"
#include "elf_dwarf_data_type.h"
#ifdef EXDEV_ENABLE
#include "shared_library.h"
#endif
#include "assert.h"
#include <string.h>
#include <stdio.h>

static Std_ReturnType Elf_Check(const Elf32_Ehdr *elf_image);
static Std_ReturnType Elf_LoadProgram(const Elf32_Ehdr *elf_image, MemoryAddressMapType *memap);
static Std_ReturnType Elf64_Check(const Elf64_Ehdr *elf_image);
static Std_ReturnType Elf64_LoadProgram(const Elf64_Ehdr *elf_image, MemoryAddressMapType *memap);

static std_bool memory_region_covers(const MemoryAddressType *region, AthrillAddrType addr, AthrillAddrType size, std_bool size_is_kb)
{
	AthrillAddrType region_size = size_is_kb ? region->size * 1024 : region->size;
	AthrillAddrType offset;

	if (addr < region->start) return FALSE;
	offset = addr - region->start;
	return offset <= region_size && size <= region_size - offset;
}

/* MALLOC regions reserve address space but have no contiguous backing store.
 * ROM, RAM and MMAP regions do, as do configured external-device regions. */
static std_bool memory_map_covers(const MemoryAddressMapType *memap, AthrillAddrType addr, AthrillAddrType size)
{
	uint32 i;

	for (i = 0; i < memap->rom_num; i++)
		if (memory_region_covers(&memap->rom[i], addr, size, TRUE)) return TRUE;
	for (i = 0; i < memap->ram_num; i++)
		if (memap->ram[i].type != MemoryAddressImplType_MALLOC &&
			memory_region_covers(&memap->ram[i], addr, size, TRUE)) return TRUE;
	for (i = 0; i < memap->dev_num; i++)
		if (memory_region_covers(&memap->dev[i], addr, size, FALSE)) return TRUE;
	return FALSE;
}

static Std_ReturnType load_segment(const uint8 *elf_data, uint64 offset, uint64 load_addr,
	uint64 file_size, uint64 memory_size, MemoryAddressMapType *memap)
{
	uint8 *ptr;

	if (memory_size < file_size || !memory_map_covers(memap, (AthrillAddrType)load_addr,
		(AthrillAddrType)memory_size)) return STD_E_NOENT;
	if (memory_size == 0) return STD_E_OK;
	if (mpu_get_pointer(CPU_CONFIG_CORE_ID_0, (AthrillAddrType)load_addr, &ptr) != STD_E_OK)
		return STD_E_INVALID;
	memcpy(ptr, elf_data + offset, (size_t)file_size);
	memset(ptr + file_size, 0, (size_t)(memory_size - file_size));
	return STD_E_OK;
}

Std_ReturnType elf_reload(uint8 *elf_data, MemoryAddressMapType *memap)
{
    const Elf32_Ehdr *elf_image = (const Elf32_Ehdr *)elf_data;
	Elf32_Phdr *phdr;
	uint8 *ptr;
	uint32 i;

	if (elf_image->e_ident[EI_CLASS] == ELFCLASS64) {
		const Elf64_Ehdr *image64 = (const Elf64_Ehdr *)elf_data;
		if (Elf64_Check(image64) != STD_E_OK) return STD_E_INVALID;
		for (i = 0; i < memap->rom_num; i++) {
			if (mpu_get_pointer(CPU_CONFIG_CORE_ID_0, memap->rom[i].start, &ptr) != STD_E_OK) return STD_E_INVALID;
			memset(ptr, 0, memap->rom[i].size * 1024);
		}
		for (i = 0; i < memap->ram_num; i++) {
			if (memap->ram[i].type == MemoryAddressImplType_MALLOC) continue;
			if (mpu_get_pointer(CPU_CONFIG_CORE_ID_0, memap->ram[i].start, &ptr) != STD_E_OK) return STD_E_INVALID;
			memset(ptr, 0, memap->ram[i].size * 1024);
		}
		for (i = 0; i < image64->e_phnum; i++) {
			const Elf64_Phdr *p = (const Elf64_Phdr *)(elf_data + image64->e_phoff + image64->e_phentsize * i);
			Std_ReturnType err;
			if (p->p_type != PT_LOAD) continue;
			err = load_segment(elf_data, p->p_offset, p->p_paddr, p->p_filesz, p->p_memsz, memap);
			if (err == STD_E_INVALID) return err;
		}
		return STD_E_OK;
	}
	if (Elf_Check(elf_image) != STD_E_OK) {
		return STD_E_INVALID;
	}
	/* elf_load() creates the MPU mappings and debug metadata.  On reset the
	 * mappings already exist, so restore only the bytes that startup created. */
	for (i = 0; i < memap->rom_num; i++) {
		if (mpu_get_pointer(CPU_CONFIG_CORE_ID_0, memap->rom[i].start, &ptr) != STD_E_OK) return STD_E_INVALID;
		memset(ptr, 0, memap->rom[i].size * 1024);
	}
	for (i = 0; i < memap->ram_num; i++) {
		if (memap->ram[i].type == MemoryAddressImplType_MALLOC) continue;
		if (mpu_get_pointer(CPU_CONFIG_CORE_ID_0, memap->ram[i].start, &ptr) != STD_E_OK) return STD_E_INVALID;
		memset(ptr, 0, memap->ram[i].size * 1024);
	}
	for (i = 0; i < elf_image->e_phnum; i++) {
		phdr = (Elf32_Phdr *)(((uint8 *)elf_image) + elf_image->e_phoff + elf_image->e_phentsize * i);
		Std_ReturnType err;
		if (phdr->p_type != PT_LOAD) continue;
		err = load_segment((const uint8 *)elf_image, phdr->p_offset, phdr->p_paddr,
			phdr->p_filesz, phdr->p_memsz, memap);
		if (err == STD_E_INVALID) return err;
	}
	return STD_E_OK;
}

Std_ReturnType binary_load(uint8 *binary_data, AthrillAddrType load_addr, uint32 binary_data_len)
{
	Std_ReturnType err;
	uint8 *data = NULL;

	err = mpu_get_pointer(CPU_CONFIG_CORE_ID_0, load_addr, &data);
	if (err != STD_E_OK) {
		return err;
	}

	memcpy(data, binary_data, binary_data_len);

	return STD_E_OK;
}

Std_ReturnType elf_load(uint8 *elf_data, MemoryAddressMapType *memap, AthrillAddrType *entry_addr)
{
	Std_ReturnType err;
	uint32 disable_debug_data_type = 0;

	if (((const Elf32_Ehdr*)elf_data)->e_ident[EI_CLASS] == ELFCLASS64) {
		const Elf64_Ehdr *image64 = (const Elf64_Ehdr *)elf_data;
		if (Elf64_Check(image64) != STD_E_OK) return STD_E_INVALID;
#if ATHRILL_ADDRESS_WIDTH == 32
		if (image64->e_entry > UINT32_MAX) {
			printf("Invalid ELF64: entry point 0x%llx is above the 32-bit Athrill bus\n", (unsigned long long)image64->e_entry);
			return STD_E_INVALID;
		}
#endif
		*entry_addr = image64->e_entry;
		err = Elf64_LoadProgram(image64, memap);
		if (err != STD_E_OK) return err;
		/* The section/symbol reader is class-aware.  DWARF consumers continue to
		 * work when the producer uses the existing 32-bit DWARF forms. */
		err = elf_symbol_load(elf_data);
		if (err != STD_E_OK) return err;
		(void)elf_dwarf_line_load(elf_data);
		(void)elf_dwarf_abbrev_load(elf_data);
		(void)elf_dwarf_info_load(elf_data);
		(void)elf_dwarf_loc_load(elf_data);
		return STD_E_OK;
	}

	*entry_addr = ((const Elf32_Ehdr*)elf_data)->e_entry;

	err = Elf_Check((const Elf32_Ehdr*)elf_data);
	if (err != STD_E_OK) {
		return err;
	}
	err = Elf_LoadProgram((const Elf32_Ehdr*)elf_data, memap);
	if (err != STD_E_OK) {
		return err;
	}
	err = elf_symbol_load(elf_data);
	if (err != STD_E_OK) {
		return err;
	}
	err = elf_dwarf_line_load(elf_data);
	if (err != STD_E_OK) {
		return err;
	}
	err = elf_dwarf_abbrev_load(elf_data);
	if (err != STD_E_OK) {
		return err;
	}
	err = elf_dwarf_info_load(elf_data);
	if (err != STD_E_OK) {
		return err;
	}
	err = elf_dwarf_loc_load(elf_data);
	if (err != STD_E_OK) {
		return err;
	}
	cpuemu_get_devcfg_value("DEBUG_FUNC_DISABLE_DEBUG_DATA_TYPE", &disable_debug_data_type);
	if (disable_debug_data_type == 0) {
		dwarf_build_data_type_set();
	}

	return err;
}


static Std_ReturnType Elf_Check(const Elf32_Ehdr *elf_image)
{
	// magic check
	if (IS_ELF(*elf_image) == FALSE) {
		return STD_E_INVALID;
	}
	if (elf_image->e_ident[EI_CLASS] != ELFCLASS32) {
		return STD_E_INVALID;
	}
	if (elf_image->e_ident[EI_DATA] != ELFDATA2LSB) {
		return STD_E_INVALID;
	}
	if (elf_image->e_ident[EI_VERSION] != EV_CURRENT) {
		return STD_E_INVALID;
	}
	if (elf_image->e_ident[EI_OSABI] != ELFOSABI_SYSV) {
		return STD_E_INVALID;
	}
	if (elf_image->e_ident[EI_ABIVERSION] != 0U) {
		return STD_E_INVALID;
	}
	if (elf_image->e_type != ET_EXEC) {
		return STD_E_INVALID;
	}
	if (elf_image->e_machine != ELF_MACHINE_TYPE) {
		return STD_E_INVALID;
	}
	if (elf_image->e_version != EV_CURRENT) {
		return STD_E_INVALID;
	}
	return STD_E_OK;
}

static Std_ReturnType Elf64_Check(const Elf64_Ehdr *elf_image)
{
	if (IS_ELF(*elf_image) == FALSE || elf_image->e_ident[EI_CLASS] != ELFCLASS64 ||
		elf_image->e_ident[EI_DATA] != ELFDATA2LSB || elf_image->e_ident[EI_VERSION] != EV_CURRENT ||
		elf_image->e_ident[EI_OSABI] != ELFOSABI_SYSV || elf_image->e_ident[EI_ABIVERSION] != 0U ||
		elf_image->e_type != ET_EXEC || elf_image->e_machine != ELF_MACHINE_TYPE ||
		elf_image->e_version != EV_CURRENT) return STD_E_INVALID;
	return STD_E_OK;
}

static void set_malloc_region(MemoryAddressMapType *memap, uint32 i)
{
	int j;
	AthrillAddrType start = memap->ram[i].start;
	uint32 unit_num = memap->ram[i].size / MPU_MALLOC_REGION_UNIT_SIZE;

	for (j = 0; j < unit_num; j++) {
		mpu_address_set_malloc_region(start, MPU_MALLOC_REGION_UNIT_SIZE * 1024);
		start += (MPU_MALLOC_REGION_UNIT_SIZE * 1024);
	}
	return;
}
static void set_cache_code_from_map(const MemoryAddressType *memory_map)
{
	if (memory_map->region_executable != FALSE) {
		virtual_cpu_cache_code_add_with_check((memory_map->size * 1024), memory_map->start);
		printf("MEMSETTING SET CACHE RIGION:addr=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT " [KB]\n", memory_map->start, memory_map->size);
	}
	return;
}
static void set_cache_code_from_elf(const Elf32_Ehdr *elf_image)
{
	uint32 i;
	Elf32_Phdr *phdr;
	for (i = 0; i < elf_image->e_phnum; i++) {
		phdr = (Elf32_Phdr*) (
				((uint8_t*)elf_image)
				+ elf_image->e_phoff
				+ (elf_image->e_phentsize * i)
				);
		//printf("p_type=0x%x\n", phdr->p_type);
		//printf("p_paddr=0x%x\n", phdr->p_paddr);
		//printf("p_flags=0x%x\n", phdr->p_flags);
		if ((phdr->p_flags & (PF_X)) == PF_X) {
			virtual_cpu_cache_code_add_with_check(phdr->p_memsz, phdr->p_vaddr);
			printf("ELF SET CACHE RIGION:addr=0x%x size=%u [KB]\n", phdr->p_vaddr, phdr->p_memsz / 1024);
		}
	}
	return;
}

static Std_ReturnType Elf_LoadProgram(const Elf32_Ehdr *elf_image, MemoryAddressMapType *memap)
{
	Std_ReturnType err;
	uint32_t i;
	Elf32_Phdr *phdr;
	uint8 *ptr = NULL;

	/*
	 * alloc memory region
	 */
	for (i = 0; i < memap->rom_num; i++) {
		ptr = mpu_address_set_rom_ram((MpuAddressGetType)memap->rom[i].type, memap->rom[i].start, memap->rom[i].size * 1024, NULL);
		if (ptr == NULL) {
			printf("Invalid elf file: can not load rom addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", memap->rom[i].start);
			return STD_E_INVALID;
		}
		set_cache_code_from_map(&memap->rom[i]);
	}
	for (i = 0; i < memap->ram_num; i++) {
		if (memap->ram[i].type != MemoryAddressImplType_MALLOC) {
			ptr = mpu_address_set_rom_ram((MpuAddressGetType)memap->ram[i].type, memap->ram[i].start, memap->ram[i].size * 1024, memap->ram[i].mmap_addr);
			if (ptr == NULL) {
				printf("Invalid elf file: can not load ram addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", memap->ram[i].start);
				return STD_E_INVALID;
			}
		}
		else {
			int j;
			AthrillAddrType start = memap->ram[i].start;
			uint32 unit_num = memap->ram[i].size / MPU_MALLOC_REGION_UNIT_SIZE;

			for (j = 0; j < unit_num; j++) {
				(void)mpu_address_set_rom_ram((MpuAddressGetType)memap->ram[i].type, start, MPU_MALLOC_REGION_UNIT_SIZE * 1024, NULL);
				start += (MPU_MALLOC_REGION_UNIT_SIZE * 1024);
			}
		}
		set_cache_code_from_map(&memap->ram[i]);
	}
	for (i = 0; i < memap->ram_num; i++) {
		if (memap->ram[i].type == MemoryAddressImplType_MALLOC) {
			set_malloc_region(memap, i);
		}
	}
#ifdef EXDEV_ENABLE
	for (i = 0; i < memap->dev_num; i++) {
		ptr = mpu_address_set_dev(memap->dev[i].start, memap->dev[i].size, memap->dev[i].extdev_handle);
		if (ptr == NULL) {
			printf("Invalid dev file: can not load dev addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", memap->dev[i].start);
			athrill_shared_library_close(memap->dev[i].extdev_handle);
			memap->dev[i].extdev_handle = NULL;
			return STD_E_INVALID;
		}
	}
#endif
	/*
	 * set cache from elf file.
	 */
	set_cache_code_from_elf(elf_image);
	/*
	 * load program from elf file.
	 */
	for (i = 0; i < elf_image->e_phnum; i++) {
		phdr = (Elf32_Phdr*) (
				((uint8_t*)elf_image)
				+ elf_image->e_phoff
				+ (elf_image->e_phentsize * i)
				);
		if (phdr->p_type != PT_LOAD) {
			continue;
		}
		/* p_paddr is the ELF load address.  In ROM-to-RAM layouts p_vaddr is
		 * the run address and startup code performs the copy from p_paddr. */
		{
			std_bool was_rom = mpu_address_get_rom(phdr->p_paddr, phdr->p_filesz) != NULL;
			err = load_segment((const uint8 *)elf_image, phdr->p_offset, phdr->p_paddr,
				phdr->p_filesz, phdr->p_memsz, memap);
			if (err == STD_E_NOENT) continue;
			if (err != STD_E_OK) {
				printf("Invalid elf file: can not load addr=0x%x\n", phdr->p_paddr);
				return STD_E_INVALID;
			}
			/* Keep the established V850 loader output unchanged. */
			if (was_rom)
				printf("Elf loading was succeeded:0x%x - 0x%x : %u.%u KB\n", phdr->p_paddr, phdr->p_paddr + phdr->p_memsz, phdr->p_filesz/1024, phdr->p_filesz % 1024);
		}
	}
	return 0;
}

static Std_ReturnType Elf64_LoadProgram(const Elf64_Ehdr *elf_image, MemoryAddressMapType *memap)
{
	Std_ReturnType err;
	uint32 i;
	uint8 *ptr = NULL;
	for (i = 0; i < memap->rom_num; i++) {
		ptr = mpu_address_set_rom_ram((MpuAddressGetType)memap->rom[i].type, memap->rom[i].start, memap->rom[i].size * 1024, NULL);
		if (ptr == NULL) return STD_E_INVALID;
		set_cache_code_from_map(&memap->rom[i]);
	}
	for (i = 0; i < memap->ram_num; i++) {
		if (memap->ram[i].type != MemoryAddressImplType_MALLOC) {
			ptr = mpu_address_set_rom_ram((MpuAddressGetType)memap->ram[i].type, memap->ram[i].start, memap->ram[i].size * 1024, memap->ram[i].mmap_addr);
			if (ptr == NULL) return STD_E_INVALID;
		} else {
			AthrillAddrType start = memap->ram[i].start;
			uint32 j, n = (uint32)(memap->ram[i].size / MPU_MALLOC_REGION_UNIT_SIZE);
			for (j = 0; j < n; j++, start += MPU_MALLOC_REGION_UNIT_SIZE * 1024)
				(void)mpu_address_set_rom_ram((MpuAddressGetType)memap->ram[i].type, start, MPU_MALLOC_REGION_UNIT_SIZE * 1024, NULL);
		}
		set_cache_code_from_map(&memap->ram[i]);
	}
	for (i = 0; i < memap->ram_num; i++) if (memap->ram[i].type == MemoryAddressImplType_MALLOC) set_malloc_region(memap, i);
	for (i = 0; i < elf_image->e_phnum; i++) {
		const Elf64_Phdr *p = (const Elf64_Phdr *)((const uint8 *)elf_image + elf_image->e_phoff + elf_image->e_phentsize * i);
		if (p->p_type != PT_LOAD) continue;
#if ATHRILL_ADDRESS_WIDTH == 32
		if (p->p_paddr > UINT32_MAX || p->p_vaddr > UINT32_MAX || p->p_filesz > UINT32_MAX || p->p_memsz > UINT32_MAX ||
			p->p_memsz > UINT64_MAX - p->p_paddr || p->p_paddr + p->p_memsz > UINT64_C(0x100000000)) {
			printf("Invalid ELF64: PT_LOAD address range 0x%llx..0x%llx is above the 32-bit Athrill bus\n",
				(unsigned long long)p->p_paddr, (unsigned long long)(p->p_paddr + p->p_memsz));
			return STD_E_INVALID;
		}
#endif
		if ((p->p_flags & PF_X) != 0) virtual_cpu_cache_code_add_with_check((uint32)p->p_memsz, p->p_vaddr);
		err = load_segment((const uint8 *)elf_image, p->p_offset, p->p_paddr,
			p->p_filesz, p->p_memsz, memap);
		if (err == STD_E_NOENT) continue;
		if (err != STD_E_OK) return STD_E_INVALID;
		if ((p->p_paddr + p->p_memsz) <= UINT32_MAX)
			printf("ELF64 loading succeeded: 0x%08x - 0x%08x\n", (uint32)p->p_paddr, (uint32)(p->p_paddr + p->p_memsz));
		else
			printf("ELF64 loading succeeded: 0x%llx - 0x%llx\n", (unsigned long long)p->p_paddr, (unsigned long long)(p->p_paddr + p->p_memsz));
	}
	return STD_E_OK;
}
