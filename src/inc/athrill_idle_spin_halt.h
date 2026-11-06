#ifndef ATHRILL_IDLE_SPIN_HALT_H
#define ATHRILL_IDLE_SPIN_HALT_H

#include "std_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Target facts used by the core's idle/spin halt policy. */
typedef struct {
	const char *name;
	/* The idle function used when DEVICE_CONFIG_IDLE_FUNCTION is absent (NULL: none). */
	const char *default_idle_function;
	std_bool (*is_branch_to_self)(CoreIdType core_id, AthrillAddrType pc);
	std_bool (*halt_ready)(CoreIdType core_id);
} AthrillIdleSpinHaltTargetOps;

void athrill_idle_spin_halt_register_target(const AthrillIdleSpinHaltTargetOps *ops);
void athrill_idle_spin_halt_init(void);

/* TRUE when the configured idle PC is current and the target says it is safe. */
std_bool athrill_idle_spin_halt_idle_ready(CoreIdType core_id);

/* Called immediately before an instruction. TRUE means the core was halted. */
std_bool athrill_idle_spin_halt_before_execute(CoreIdType core_id);

/* Called after an instruction at pc has committed its next PC. */
void athrill_idle_spin_halt_after_execute(CoreIdType core_id, AthrillAddrType pc);

#ifdef __cplusplus
}
#endif

#endif /* ATHRILL_IDLE_SPIN_HALT_H */
