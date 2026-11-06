#ifndef ATHRILL_HOST_FUNCTION_INTERNAL_H
#define ATHRILL_HOST_FUNCTION_INTERNAL_H

#include "athrill_host_function.h"
#include "std_errno.h"

typedef struct {
	const char *name;
	Std_ReturnType (*arg)(CoreIdType core_id, uint32 index, uint64 *value);
	void (*set_result)(CoreIdType core_id, uint64 value);
	void (*return_from_call)(CoreIdType core_id);
	AthrillAddrType (*canonical_address)(AthrillAddrType address);
	void (*invalidate_code)(AthrillAddrType address, uint32 size);
} AthrillHostFunctionTargetOps;

void athrill_host_function_register_target(const AthrillHostFunctionTargetOps *ops);
Std_ReturnType athrill_host_function_init(void);
int athrill_host_function_find(AthrillAddrType address);
std_bool athrill_host_function_invoke(CoreIdType core_id, int function_id);
Std_ReturnType athrill_host_function_memory_read(CoreIdType core_id,
	AthrillAddrType address, void *buffer, uint32 size);

#endif /* ATHRILL_HOST_FUNCTION_INTERNAL_H */
