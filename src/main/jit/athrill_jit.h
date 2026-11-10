#ifndef ATHRILL_JIT_H
#define ATHRILL_JIT_H

/*
 * Athrill JIT (experiment): the target-independent part.
 *
 * The core gives a target two things:
 *
 *   1. A block builder. A target describes one block of guest instructions
 *      with these operations; the builder turns the block into one host
 *      function (with sljit). The operations work on the target's own CPU
 *      structure ("core fields"), a per-call context ("ctx fields"),
 *      absolute host addresses and immediates. The target chooses, per
 *      guest instruction, to emit native operations or to call its existing
 *      C execute function (call-threaded); both can be mixed in one block.
 *
 *   2. A block cache: compiled blocks by (pc, key), dropped when the
 *      target's generation for that code changes.
 *
 * What the core does NOT decide: guest semantics (flags, shift ranges,
 * division by zero, traps), when a block ends, and the clock and counter
 * rules. The target writes those with the builder operations, so the same
 * rules as its interpreter hold.
 *
 * A compiled block is called as
 *     uint64 executed = athrill_jit_code_run(code, core, ctx);
 * It returns the "done" counter: the number the target counted with
 * athrill_jit_count() before the block exited.
 *
 * Host safety: no operation lets the host trap. Division by zero and the
 * signed overflow (MIN / -1) must be guarded by the target with
 * athrill_jit_jump_if() before athrill_jit_alu() with DIVU, DIVS, REMU or REMS; the builder
 * refuses a division whose divisor is an immediate 0 or -1 for the same
 * reason. Shift counts are masked by (width - 1) on every host, as in
 * RISC-V; a target whose ISA differs (Arm: register shifts of 32..255)
 * must guard the range itself.
 */

#include "std_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AthrillJitBuilder AthrillJitBuilder;
typedef struct AthrillJitCode AthrillJitCode;
typedef struct AthrillJitLabel AthrillJitLabel;

/* ---- operands ---- */
typedef enum {
	ATHRILL_JIT_OPERAND_IMM = 0,	/* an immediate value */
	ATHRILL_JIT_OPERAND_CORE,	/* a field of the target's CPU structure (offset) */
	ATHRILL_JIT_OPERAND_CTX,	/* a field of the per-call context (offset) */
	ATHRILL_JIT_OPERAND_ABS,	/* a host variable (absolute address) */
	ATHRILL_JIT_OPERAND_DONE,	/* the block's "done" counter (64 bit) */
	ATHRILL_JIT_OPERAND_RESULT,	/* the int returned by the last athrill_jit_call() */
	ATHRILL_JIT_OPERAND_TEMP,	/* a host register for the target's own values (index) */
} AthrillJitOperandKind;

/*
 * Temporaries: ATHRILL_JIT_TEMPS host registers (callee-saved, so a value
 * survives athrill_jit_call()) for values the target computes and reuses
 * within a block, instead of fields in memory. Their value at block entry
 * is undefined.
 */
#define ATHRILL_JIT_TEMPS 2U

typedef struct {
	AthrillJitOperandKind kind;
	uint8 width;	/* 4 or 8 bytes (IMM: the width of the operation) */
	sint64 value;	/* IMM: the value; CORE/CTX: the offset; ABS: the address */
} AthrillJitOperand;

AthrillJitOperand athrill_jit_imm(sint64 value);
AthrillJitOperand athrill_jit_core(uint32 offset, uint8 width);
AthrillJitOperand athrill_jit_ctx(uint32 offset, uint8 width);
AthrillJitOperand athrill_jit_abs(const void *address, uint8 width);
AthrillJitOperand athrill_jit_done(void);
AthrillJitOperand athrill_jit_result(void);
AthrillJitOperand athrill_jit_temp(uint32 index, uint8 width);	/* index < ATHRILL_JIT_TEMPS */

/* ---- operations ---- */
typedef enum {
	ATHRILL_JIT_ADD = 0,
	ATHRILL_JIT_SUB,
	ATHRILL_JIT_MUL,	/* the low half of the product */
	ATHRILL_JIT_AND,
	ATHRILL_JIT_OR,
	ATHRILL_JIT_XOR,
	ATHRILL_JIT_SHL,	/* count masked by (width - 1) */
	ATHRILL_JIT_SHR,	/* logical; count masked by (width - 1) */
	ATHRILL_JIT_SAR,	/* arithmetic; count masked by (width - 1) */
	ATHRILL_JIT_DIVU,	/* the divisor must be guarded (not 0) */
	ATHRILL_JIT_DIVS,	/* the divisor must be guarded (not 0, not MIN / -1) */
	ATHRILL_JIT_REMU,	/* the divisor must be guarded (not 0) */
	ATHRILL_JIT_REMS,	/* the divisor must be guarded (not 0, not MIN / -1) */
} AthrillJitAluOp;

typedef enum {
	ATHRILL_JIT_EQ = 0,
	ATHRILL_JIT_NE,
	ATHRILL_JIT_LTU,
	ATHRILL_JIT_GEU,
	ATHRILL_JIT_GTU,
	ATHRILL_JIT_LEU,
	ATHRILL_JIT_LTS,
	ATHRILL_JIT_GES,
	ATHRILL_JIT_GTS,
	ATHRILL_JIT_LES,
} AthrillJitCond;

/*
 * The C function a block calls (a target's execute function or a wrapper
 * around it). Its int result is kept as the RESULT operand.
 */
typedef int (*AthrillJitCallFunc)(void *core, void *ctx, const void *arg);

/* ---- building a block ---- */

/* NULL when the JIT is not available on this host. */
AthrillJitBuilder *athrill_jit_builder_create(void);

/*
 * dst = src. dst is CORE, CTX, ABS or TEMP. A 4-byte src into an 8-byte dst is
 * zero-extended by athrill_jit_mov() and sign-extended by athrill_jit_movs();
 * an 8-byte src into a 4-byte dst keeps the low half.
 */
void athrill_jit_mov(AthrillJitBuilder *b, AthrillJitOperand dst, AthrillJitOperand src);
void athrill_jit_movs(AthrillJitBuilder *b, AthrillJitOperand dst, AthrillJitOperand src);

/*
 * dst = a op c. All operands have the operation's width: the width of dst
 * (4: the guest's 32-bit operation; the upper half of a 64-bit dst is not
 * involved). Immediates are truncated to the width.
 */
void athrill_jit_alu(AthrillJitBuilder *b, AthrillJitAluOp op,
		     AthrillJitOperand dst, AthrillJitOperand a, AthrillJitOperand c);

/*
 * Host memory through a computed address: the access is at the host
 * address (bias + addr), both 8-byte operands; bias is a host address the
 * target worked out (e.g. its RAM buffer minus the guest base). size is
 * 1, 2, 4 or 8 bytes, little endian, any alignment.
 *
 * The core does not check the address: the target guards it first
 * (athrill_jit_jump_if() on its own range) so that only host memory it
 * owns is reached.
 *
 * load: dst (8 bytes) = the value, zero-extended, or sign-extended when
 * sign_extend. store: the low size bytes of src.
 */
void athrill_jit_load_host(AthrillJitBuilder *b, AthrillJitOperand dst, AthrillJitOperand addr,
			   AthrillJitOperand bias, uint8 size, std_bool sign_extend);
void athrill_jit_store_host(AthrillJitBuilder *b, AthrillJitOperand src, AthrillJitOperand addr,
			    AthrillJitOperand bias, uint8 size);

/* done = done + n */
void athrill_jit_count(AthrillJitBuilder *b, uint32 n);

/* RESULT = func(core, ctx, arg). The scratch state is lost; fields are not. */
void athrill_jit_call(AthrillJitBuilder *b, AthrillJitCallFunc func, const void *arg);

/*
 * RESULT = func(core, value): a target's own function of that form (an
 * instruction's executor with its encoding), called directly.
 */
typedef int (*AthrillJitCallValueFunc)(void *core, uint32 value);
void athrill_jit_call_value(AthrillJitBuilder *b, AthrillJitCallValueFunc func, uint32 value);

/* Labels and jumps (within one block). */
AthrillJitLabel *athrill_jit_label_new(AthrillJitBuilder *b);
void athrill_jit_label_place(AthrillJitBuilder *b, AthrillJitLabel *label);
void athrill_jit_jump(AthrillJitBuilder *b, AthrillJitLabel *label);
/* Compares with the width of a (an IMM a takes the width of c). */
void athrill_jit_jump_if(AthrillJitBuilder *b, AthrillJitCond cond,
			 AthrillJitOperand a, AthrillJitOperand c, AthrillJitLabel *label);

/* dst = (a cond c) ? 1 : 0, compared with the width of a (an IMM a takes c's). */
void athrill_jit_set_if(AthrillJitBuilder *b, AthrillJitCond cond, AthrillJitOperand dst,
			AthrillJitOperand a, AthrillJitOperand c);

/* Returns from the block with the done counter. */
void athrill_jit_exit(AthrillJitBuilder *b);
/* if (a cond c) exit */
void athrill_jit_exit_if(AthrillJitBuilder *b, AthrillJitCond cond,
			 AthrillJitOperand a, AthrillJitOperand c);

/*
 * Ends the description and generates the code. The builder is freed.
 * NULL when any operation was invalid or the code could not be generated
 * (athrill_jit_last_error() says why); the caller then runs the
 * instructions in its interpreter.
 */
AthrillJitCode *athrill_jit_builder_finish(AthrillJitBuilder *b);
/* Drops a builder without generating code. */
void athrill_jit_builder_abort(AthrillJitBuilder *b);
const char *athrill_jit_last_error(void);

/*
 * Chaining: an exit that can later go straight on to another block.
 *
 * athrill_jit_chain_exit() ends the block like athrill_jit_exit(), through
 * a link slot. Unlinked, it stores the slot's address in slot_out (an
 * 8-byte CORE or CTX field) and returns the done counter. The target then
 * calls athrill_jit_chain_link(slot, next) when it knows the block that
 * runs next after that exit (tag: the target's own value for the slot,
 * e.g. the guest pc the exit goes to, read back with
 * athrill_jit_chain_tag()). Linked, the exit jumps to next's chain entry
 * without returning: the done counter and RESULT go on, so next's first
 * operations see the counts of the blocks before it; the target emits its
 * entry checks (budget, ...) as the first operations of every block.
 *
 * Every athrill_jit_code_free() (and so every cache drop or flush) first
 * unlinks all slots, so no link points to freed code. The target calls
 * athrill_jit_chain_unlink_all() itself whenever a link may have become
 * wrong (its code changed, its mode changed).
 */
typedef struct AthrillJitChainSlot AthrillJitChainSlot;

void athrill_jit_chain_exit(AthrillJitBuilder *b, AthrillJitOperand slot_out, uint64 tag);
uint64 athrill_jit_chain_tag(const AthrillJitChainSlot *slot);
void athrill_jit_chain_link(AthrillJitChainSlot *slot, const AthrillJitCode *next);
void athrill_jit_chain_unlink_all(void);

/*
 * Indirect chaining, for an exit whose next pc is only known at run time
 * (a register jump). athrill_jit_chain_indirect() looks the pc (an 8-byte
 * operand, an even value) up in one table of (pc -> block) the core keeps;
 * found, it jumps to that block's chain entry as a linked exit does; not
 * found, the block goes on with the next operation (the target then exits
 * and, in C, enters the pair with athrill_jit_chain_indirect_set()).
 * The table holds ATHRILL_JIT_INDIRECT_ENTRIES pairs, one per (pc / 2) mod
 * its size; athrill_jit_chain_unlink_all() (so every code free) empties it.
 */
#define ATHRILL_JIT_INDIRECT_ENTRIES 4096U
void athrill_jit_chain_indirect(AthrillJitBuilder *b, AthrillJitOperand pc);
void athrill_jit_chain_indirect_set(uint64 pc, const AthrillJitCode *next);
uint32 athrill_jit_chain_links(void);	/* slots linked now */

/* ---- running ---- */
uint64 athrill_jit_code_run(const AthrillJitCode *code, void *core, void *ctx);
uint32 athrill_jit_code_size(const AthrillJitCode *code);
/* The host code itself (for inspection: dumping and disassembling). */
const void *athrill_jit_code_entry(const AthrillJitCode *code);
void athrill_jit_code_free(AthrillJitCode *code);

/* ---- block cache ---- */
typedef struct AthrillJitCache AthrillJitCache;

typedef struct {
	uint64 lookups;
	uint64 hits;
	uint64 stale;		/* found with an old generation (dropped) */
	uint64 inserts;
	uint64 flushes;		/* whole-cache flushes because it was full */
	uint32 blocks;		/* blocks held now */
	uint64 code_bytes;	/* host code held now */
} AthrillJitCacheStats;

/* capacity: the number of blocks it holds (rounded up to a power of 2). */
AthrillJitCache *athrill_jit_cache_create(uint32 capacity);
void athrill_jit_cache_destroy(AthrillJitCache *cache);

/*
 * The block for (pc, key) compiled under generation, or NULL. A block of
 * another generation is freed and counted as stale. key is the target's
 * mode at the pc (privilege, instruction set, ...); generation is the
 * target's counter that changes when the code bytes may have changed.
 */
const AthrillJitCode *athrill_jit_cache_lookup(AthrillJitCache *cache,
					       AthrillAddrType pc, uint32 key, uint64 generation);
/* Takes ownership of code. When the cache is full, all blocks are freed first. */
void athrill_jit_cache_insert(AthrillJitCache *cache, AthrillAddrType pc, uint32 key,
			      uint64 generation, AthrillJitCode *code);
/* Frees every block (e.g. the target lost track of its code). */
void athrill_jit_cache_flush(AthrillJitCache *cache);
void athrill_jit_cache_get_stats(const AthrillJitCache *cache, AthrillJitCacheStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* ATHRILL_JIT_H */
