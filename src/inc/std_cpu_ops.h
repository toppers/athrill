#ifndef _STD_CPU_OPS_H_
#define _STD_CPU_OPS_H_

#include "std_types.h"
#include "std_errno.h"
#include "cpu_config_ops.h"

extern void cpu_init(void);
extern void cpu_reset(CoreIdType core_id);
extern void cpu_illegal_opcode_trap(CoreIdType core_id);
extern void cpu_set_current_core(CoreIdType core_id);
extern Std_ReturnType cpu_supply_clock(CoreIdType core_id);
extern std_bool cpu_is_halt(CoreIdType core_id);
extern std_bool cpu_is_halt_all(void);
extern void cpu_mpu_construct_containers(CoreIdType core_id);
extern std_bool cpu_illegal_access(CoreIdType core_id);
/*
 * Batched execution (targets that define ATHRILL_TARGET_BATCH_CLOCKS > 1):
 * run up to max_clocks further instructions, one clock each, without
 * device ticks. Before each instruction the target advances *clock (the
 * core's virtual clock, read by devices on access). It stops early when
 * the CPU halts or touches a device. *executed is the number run; an
 * error is reported as by cpu_supply_clock().
 */
extern Std_ReturnType cpu_supply_clocks(CoreIdType core_id, uint64 max_clocks, uint64 *clock,
	uint64 *executed);

#endif /* _STD_CPU_OPS_H_ */
