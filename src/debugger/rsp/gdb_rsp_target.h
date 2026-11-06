#ifndef ATHRILL_GDB_RSP_TARGET_H
#define ATHRILL_GDB_RSP_TARGET_H

#include "std_types.h"

/* Target adapter required when ATHRILL_ENABLE_GDB_RSP is enabled. */

/* Return the number of registers in the target's GDB register image. */
extern uint32 dbg_target_gdb_reg_count(void);

/* Return the size in bytes of register regno. */
extern uint32 dbg_target_gdb_reg_size(uint32 regno);

/* Read or write register regno using its target-defined byte representation.
 * The caller provides dbg_target_gdb_reg_size(regno) bytes. Return FALSE when
 * the register is unavailable or cannot be accessed.
 */
extern std_bool dbg_target_gdb_read_reg(uint32 core_id, uint32 regno, uint8 *bytes);
extern std_bool dbg_target_gdb_write_reg(uint32 core_id, uint32 regno, const uint8 *bytes);

/* Set the target program counter to pc. */
extern void dbg_target_gdb_set_pc(uint32 core_id, uint32 pc);

#endif /* ATHRILL_GDB_RSP_TARGET_H */
