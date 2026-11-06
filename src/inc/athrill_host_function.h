/* Public interface for user-written host functions. */
#ifndef ATHRILL_HOST_FUNCTION_H
#define ATHRILL_HOST_FUNCTION_H

#include "std_types.h"
#include "std_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AthrillHostCall AthrillHostCall;

struct AthrillHostCall {
	uint64 (*arg)(AthrillHostCall *call, uint32 index);
	void (*set_result)(AthrillHostCall *call, uint64 value);
	Std_ReturnType (*read)(AthrillHostCall *call, uint64 addr, void *buf, uint32 size);
	Std_ReturnType (*write)(AthrillHostCall *call, uint64 addr, const void *buf, uint32 size);
	void *context;
};

/* FALSE asks Athrill to execute the firmware function normally. */
typedef std_bool (*AthrillHostFunction)(AthrillHostCall *call);

#ifdef __cplusplus
}
#endif

#endif /* ATHRILL_HOST_FUNCTION_H */
