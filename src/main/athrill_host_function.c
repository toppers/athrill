#include "athrill_host_function_internal.h"

#include "cpuemu_ops.h"
#include "mpu_ops.h"
#include "shared_library.h"
#include "symbol_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST_FUNCTION_CONFIG "DEVICE_CONFIG_HOST_FUNCTION"
#define HOST_FUNCTION_LIBRARY_CONFIG "DEVICE_CONFIG_HOST_FUNCTION_LIBRARY"
#define HOST_FUNCTION_MAX 64U

typedef struct {
	const char *guest_name;
	const char *host_name;
	AthrillAddrType address;
	AthrillHostFunction function;
	uint64 handled;
	uint64 refused;
} HostFunctionEntry;

typedef struct {
	CoreIdType core_id;
	std_bool failed;
	std_bool result_set;
	uint64 result;
} HostCallContext;

static const AthrillHostFunctionTargetOps *target_ops;
static AthrillSharedLibraryHandle host_library;
static HostFunctionEntry host_functions[HOST_FUNCTION_MAX];
static uint32 host_function_count;

void athrill_host_function_register_target(const AthrillHostFunctionTargetOps *ops)
{
	target_ops = ops;
}

static uint64 host_call_arg(AthrillHostCall *call, uint32 index)
{
	HostCallContext *context = (HostCallContext *)call->context;
	uint64 value = 0U;

	if (context->failed || (target_ops->arg(context->core_id, index, &value) != STD_E_OK)) {
		context->failed = TRUE;
		return 0U;
	}
	return value;
}

static void host_call_set_result(AthrillHostCall *call, uint64 value)
{
	HostCallContext *context = (HostCallContext *)call->context;

	context->result = value;
	context->result_set = TRUE;
}

static Std_ReturnType checked_address(uint64 address, uint32 size, AthrillAddrType *converted)
{
	if ((address > (uint64)ATHRILL_ADDR_MAX)
		|| ((size != 0U) && (address > ((uint64)ATHRILL_ADDR_MAX - ((uint64)size - 1U))))) {
		return STD_E_SEGV;
	}
	*converted = (AthrillAddrType)address;
	return STD_E_OK;
}

Std_ReturnType athrill_host_function_memory_read(CoreIdType core_id,
	AthrillAddrType address, void *buffer, uint32 size)
{
	uint8 *source;
	Std_ReturnType err;

	if ((buffer == NULL) && (size != 0U)) {
		return STD_E_INVALID;
	}
	err = mpu_get_host_memory(core_id, address, size, FALSE, &source);
	if ((err == STD_E_OK) && (size != 0U)) {
		memcpy(buffer, source, size);
	}
	return err;
}

static Std_ReturnType host_call_read(AthrillHostCall *call, uint64 address,
	void *buffer, uint32 size)
{
	HostCallContext *context = (HostCallContext *)call->context;
	AthrillAddrType converted;
	Std_ReturnType err = checked_address(address, size, &converted);

	if (err == STD_E_OK) {
		err = athrill_host_function_memory_read(context->core_id, converted, buffer, size);
	}
	if (err != STD_E_OK) {
		context->failed = TRUE;
	}
	return err;
}

static Std_ReturnType host_call_write(AthrillHostCall *call, uint64 address,
	const void *buffer, uint32 size)
{
	HostCallContext *context = (HostCallContext *)call->context;
	AthrillAddrType converted;
	uint8 *destination;
	Std_ReturnType err;

	if ((buffer == NULL) && (size != 0U)) {
		context->failed = TRUE;
		return STD_E_INVALID;
	}
	err = checked_address(address, size, &converted);
	if (err == STD_E_OK) {
		err = mpu_get_host_memory(context->core_id, converted, size, TRUE, &destination);
	}
	if (err != STD_E_OK) {
		context->failed = TRUE;
		return err;
	}
	if (size != 0U) {
		memcpy(destination, buffer, size);
		target_ops->invalidate_code(converted, size);
	}
	return STD_E_OK;
}

static void print_host_function_counts(void)
{
	uint32 i;

	for (i = 0U; i < host_function_count; i++) {
		fprintf(stderr, "HOST_FUNCTION_CALLS %s handled=%llu refused=%llu\n",
			host_functions[i].guest_name,
			(unsigned long long)host_functions[i].handled,
			(unsigned long long)host_functions[i].refused);
	}
}

static Std_ReturnType validate_target(void)
{
	if ((target_ops == NULL) || (target_ops->arg == NULL) || (target_ops->set_result == NULL)
		|| (target_ops->return_from_call == NULL) || (target_ops->canonical_address == NULL)
		|| (target_ops->invalidate_code == NULL)) {
		fprintf(stderr, "ERROR: this target does not support DEVICE_CONFIG_HOST_FUNCTION\n");
		return STD_E_INVALID;
	}
	return STD_E_OK;
}

Std_ReturnType athrill_host_function_init(void)
{
	char *library_path = NULL;
	char *guest_name;
	char *host_name;
	char error_message[512];
	AthrillAddrType address;
	uint32 size;
	uint32 i;
	union {
		void *object;
		AthrillHostFunction function;
	} symbol;

	if (cpuemu_get_devcfg_string(HOST_FUNCTION_LIBRARY_CONFIG, &library_path) != STD_E_OK) {
		if (cpuemu_get_devcfg_string_pair_at(HOST_FUNCTION_CONFIG, 0U,
			&guest_name, &host_name) == STD_E_OK) {
			fprintf(stderr, "ERROR: %s needs %s\n", HOST_FUNCTION_CONFIG,
				HOST_FUNCTION_LIBRARY_CONFIG);
			return STD_E_INVALID;
		}
		return STD_E_OK;
	}
	if (validate_target() != STD_E_OK) {
		return STD_E_INVALID;
	}
	host_library = athrill_shared_library_open(library_path,
		error_message, sizeof(error_message));
	if (host_library == NULL) {
		fprintf(stderr, "ERROR: cannot load host-function library %s: %s\n",
			library_path, error_message);
		return STD_E_NOENT;
	}

	for (i = 0U; i < HOST_FUNCTION_MAX; i++) {
		if (cpuemu_get_devcfg_string_pair_at(HOST_FUNCTION_CONFIG, i,
			&guest_name, &host_name) != STD_E_OK) {
			break;
		}
		if (symbol_get_func(guest_name, (uint32)strlen(guest_name), &address, &size) < 0) {
			fprintf(stderr, "ERROR: firmware function symbol not found: %s\n", guest_name);
			return STD_E_NOENT;
		}
		address = target_ops->canonical_address(address);
		for (uint32 j = 0U; j < i; j++) {
			if (host_functions[j].address == address) {
				fprintf(stderr, "ERROR: duplicate host-function address 0x%" ATHRILL_ADDR_HEX_FORMAT
					" (%s and %s)\n", address, host_functions[j].guest_name, guest_name);
				return STD_E_INVALID;
			}
		}
		symbol.object = athrill_shared_library_symbol(host_library, host_name,
			error_message, sizeof(error_message));
		if (symbol.object == NULL) {
			fprintf(stderr, "ERROR: host function not found: %s: %s\n", host_name, error_message);
			return STD_E_NOENT;
		}
		host_functions[i].guest_name = guest_name;
		host_functions[i].host_name = host_name;
		host_functions[i].address = address;
		host_functions[i].function = symbol.function;
		host_function_count++;
		printf("HOST FUNCTION %s at 0x%" ATHRILL_ADDR_HEX_FORMAT " -> %s\n",
			guest_name, address, host_name);
	}
	if (host_function_count == 0U) {
		fprintf(stderr, "ERROR: %s is set but no %s entries were found\n",
			HOST_FUNCTION_LIBRARY_CONFIG, HOST_FUNCTION_CONFIG);
		return STD_E_INVALID;
	}
	if (i == HOST_FUNCTION_MAX) {
		if (cpuemu_get_devcfg_string_pair_at(HOST_FUNCTION_CONFIG, i,
			&guest_name, &host_name) == STD_E_OK) {
			fprintf(stderr, "ERROR: at most %u host functions are supported\n", HOST_FUNCTION_MAX);
			return STD_E_INVALID;
		}
	}
	(void)atexit(print_host_function_counts);
	return STD_E_OK;
}

int athrill_host_function_find(AthrillAddrType address)
{
	uint32 i;

	if ((host_function_count == 0U) || (target_ops == NULL)) {
		return -1;
	}
	address = target_ops->canonical_address(address);
	for (i = 0U; i < host_function_count; i++) {
		if (host_functions[i].address == address) {
			return (int)i;
		}
	}
	return -1;
}

std_bool athrill_host_function_invoke(CoreIdType core_id, int function_id)
{
	HostCallContext context;
	AthrillHostCall call;
	std_bool handled;
	HostFunctionEntry *entry;

	if ((function_id < 0) || ((uint32)function_id >= host_function_count)) {
		return FALSE;
	}
	entry = &host_functions[function_id];
	memset(&context, 0, sizeof(context));
	context.core_id = core_id;
	call.arg = host_call_arg;
	call.set_result = host_call_set_result;
	call.read = host_call_read;
	call.write = host_call_write;
	call.context = &context;
	handled = entry->function(&call);
	if (!handled || context.failed) {
		entry->refused++;
		return FALSE;
	}
	if (context.result_set) {
		target_ops->set_result(core_id, context.result);
	}
	target_ops->return_from_call(core_id);
	entry->handled++;
	return TRUE;
}
