#ifndef _DBG_PRINT_DATA_TYPE_H_
#define _DBG_PRINT_DATA_TYPE_H_

#include "std_types.h"

extern std_bool print_variable_with_data_type(char *variable_name, AthrillAddrType vaddr, uint8 *top_addr, uint32 size);
extern std_bool print_addr_with_data_type(AthrillAddrType vaddr, uint8 *top_addr, char* dataType, char* dataTypeName);
extern std_bool print_local_variable_type(char *variable_name);

#endif /* _DBG_PRINT_DATA_TYPE_H_ */
