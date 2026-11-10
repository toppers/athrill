#include "athrill_idle_spin_halt.h"

#include "cpu.h"
#include "cpuemu_ops.h"
#include "symbol_ops.h"
#include "std_errno.h"

#include <stdio.h>
#include <string.h>

#define IDLE_FUNCTION_CONFIG "DEVICE_CONFIG_IDLE_FUNCTION"
#define SPIN_HALT_CONFIG "DEVICE_CONFIG_SPIN_HALT"
#define ARM_SPIN_HALT_CONFIG "DEVICE_CONFIG_ARM_SPIN_HALT"

static const AthrillIdleSpinHaltTargetOps *target_ops;
static AthrillAddrType idle_pc;
static std_bool spin_halt;

void athrill_idle_spin_halt_register_target(const AthrillIdleSpinHaltTargetOps *ops)
{
	target_ops = ops;
}

static std_bool target_has_idle_halt(void)
{
	return (target_ops != NULL) && (target_ops->halt_ready != NULL);
}

static std_bool target_has_spin_halt(void)
{
	return target_has_idle_halt() && (target_ops->is_branch_to_self != NULL);
}

static void read_idle_function(void)
{
	char *name = NULL;
	AthrillAddrType address;
	uint32 size;
	uint32 skip = 0U;
	std_bool configured;

	configured = cpuemu_get_devcfg_string(IDLE_FUNCTION_CONFIG, &name) == STD_E_OK;
	if (!configured && (target_ops != NULL)) {
		/* The target's default keeps configurations without the line working as before. */
		name = (char *)target_ops->default_idle_function;
	}
	if ((name == NULL) || (strcmp(name, "none") == 0)) {
		return;
	}
	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_ENABLE_SKIP_CLOCK", &skip);
	if (skip != 1U) {
		fprintf(stderr, "INFO: no clock skip: the idle function is not used\n");
		return;
	}
	if (!target_has_idle_halt()) {
		fprintf(stderr, "WARNING: %s is set, but this target does not provide idle-function halt\n",
			IDLE_FUNCTION_CONFIG);
		return;
	}
	if (symbol_get_func(name, (uint32)strlen(name), &address, &size) < 0) {
		if (configured) {
			fprintf(stderr, "WARNING: %s %s: no such function in the ELF; the idle function is not used\n",
				IDLE_FUNCTION_CONFIG, name);
		} else {
			fprintf(stderr, "INFO: no %s in the ELF: the idle function is not used\n", name);
		}
		return;
	}
	/* All supported instruction sets require bit zero of an instruction PC to be clear. */
	idle_pc = address & ~(AthrillAddrType)1U;
	fprintf(stderr, "INFO: idle function at 0x%" ATHRILL_ADDR_HEX_PADDED_FORMAT "\n", idle_pc);
}

static void read_spin_halt(void)
{
	uint32 enabled = 0U;

	if (cpuemu_get_devcfg_value(SPIN_HALT_CONFIG, &enabled) != STD_E_OK) {
		(void)cpuemu_get_devcfg_value(ARM_SPIN_HALT_CONFIG, &enabled);
	}
	spin_halt = enabled != 0U;
	if (!spin_halt) {
		return;
	}
	if (!target_has_spin_halt()) {
		fprintf(stderr, "WARNING: branch-to-self halt is set, but this target does not provide spin-halt\n");
		spin_halt = FALSE;
		return;
	}
	fprintf(stderr, "INFO: a branch to itself halts the core as WFI\n");
}

void athrill_idle_spin_halt_init(void)
{
	const char *name = (target_ops != NULL) && (target_ops->name != NULL)
		? target_ops->name : "unregistered";

	idle_pc = 0U;
	spin_halt = FALSE;
	fprintf(stderr, "INFO: %s halt capabilities: idle-function=%s spin-halt=%s\n",
		name,
		target_has_idle_halt() ? "yes" : "no",
		target_has_spin_halt() ? "yes" : "no");
	read_idle_function();
	read_spin_halt();
}

std_bool athrill_idle_spin_halt_idle_ready(CoreIdType core_id)
{
	TargetCoreType *core;

	if ((idle_pc == 0U) || !target_has_idle_halt()) {
		return FALSE;
	}
	core = &virtual_cpu.cores[core_id].core;
	return (cpu_get_pc(core) == idle_pc) && target_ops->halt_ready(core_id);
}

std_bool athrill_idle_spin_halt_before_execute(CoreIdType core_id)
{
	if (!athrill_idle_spin_halt_idle_ready(core_id)) {
		return FALSE;
	}
	virtual_cpu.cores[core_id].core.is_halt = TRUE;
	return TRUE;
}

void athrill_idle_spin_halt_after_execute(CoreIdType core_id, AthrillAddrType pc)
{
	TargetCoreType *core;

	if (!spin_halt) {
		return;
	}
	core = &virtual_cpu.cores[core_id].core;
	if ((cpu_get_pc(core) == pc) && target_ops->halt_ready(core_id)
		&& target_ops->is_branch_to_self(core_id, pc)) {
		core->is_halt = TRUE;
	}
}
