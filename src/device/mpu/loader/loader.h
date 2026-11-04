#ifndef _LOADER_H_
#define _LOADER_H_

#include "std_errno.h"
#include "std_types.h"
#include "cpuemu_ops.h"

extern Std_ReturnType binary_load(uint8 *binary_data, AthrillAddrType load_addr, uint32 binary_data_len);
extern Std_ReturnType elf_load(uint8 *elf_data, uint64 elf_size, MemoryAddressMapType *memap, AthrillAddrType *entry_addr);
extern Std_ReturnType elf_reload(uint8 *elf_data, uint64 elf_size, MemoryAddressMapType *memap);

#endif /* _LOADER_H_ */
