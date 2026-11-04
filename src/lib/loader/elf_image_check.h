#ifndef _ELF_IMAGE_CHECK_H_
#define _ELF_IMAGE_CHECK_H_

#include "std_types.h"
#include "std_errno.h"

/*
 * Check that an ELF file image is well formed before anything reads it.
 *
 * The loader, the symbol reader and the DWARF readers follow offsets and
 * sizes from the file without bounds checks of their own. This function
 * makes sure that every structure they use lies inside the file:
 *   - the ELF header (32 or 64 bit, little endian);
 *   - the program header table and the file part of every PT_LOAD;
 *   - the section header table, every section with file contents, the
 *     section name table and every section name;
 *   - every symbol table, its string table and every symbol name.
 * Returns STD_E_OK or STD_E_INVALID; the reason is printed.
 */
extern Std_ReturnType elf_image_check(const uint8 *elf_data, uint64 elf_size);

#endif /* _ELF_IMAGE_CHECK_H_ */
