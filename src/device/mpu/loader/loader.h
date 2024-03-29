#ifndef _LOADER_H_
#define _LOADER_H_

#include "std_errno.h"
#include "std_types.h"
#include "cpuemu_ops.h"

extern Std_ReturnType binary_load(uint8 *binary_data, uint32 load_addr, uint32 binary_data_len);
extern Std_ReturnType elf_load(uint8 *elf_data, MemoryAddressMapType *memap, uint32* entry_addr);

#endif /* _LOADER_H_ */
