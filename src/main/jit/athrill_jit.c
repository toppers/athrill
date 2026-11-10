/*
 * Athrill JIT (experiment): block builder and block cache over sljit.
 * See athrill_jit.h for the contract.
 *
 * Register use in a generated block:
 *   S0 = core, S1 = ctx (the two arguments), S2 = done counter,
 *   S3 = RESULT of the last call; R0..R2 are scratch.
 */
#include "athrill_jit.h"
#include "sljitLir.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JIT_REG_CORE	SLJIT_S0
#define JIT_REG_CTX	SLJIT_S1
#define JIT_REG_DONE	SLJIT_S2
#define JIT_REG_RESULT	SLJIT_S3

static char jit_error[160];

struct AthrillJitLabel {
	struct sljit_label *placed;
	struct sljit_jump **pending;
	uint32 pending_num;
	uint32 pending_cap;
	AthrillJitLabel *next_in_builder;
};

struct AthrillJitBuilder {
	struct sljit_compiler *c;
	std_bool failed;
	AthrillJitLabel *labels;
	/* every exit jumps here: one return sequence per block */
	AthrillJitLabel *exit_label;
};

struct AthrillJitCode {
	void *entry;
	uint32 size;
};

typedef sljit_sw (*JitEntryFunc)(void *core, void *ctx);

static void set_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vsnprintf(jit_error, sizeof(jit_error), fmt, ap);
	va_end(ap);
}

static void fail(AthrillJitBuilder *b, const char *what)
{
	if (!b->failed) {
		set_error("%s", what);
	}
	b->failed = TRUE;
}

const char *athrill_jit_last_error(void)
{
	return jit_error;
}

/* ---- operands ---- */

static AthrillJitOperand operand(AthrillJitOperandKind kind, uint8 width, sint64 value)
{
	AthrillJitOperand op;

	op.kind = kind;
	op.width = width;
	op.value = value;
	return op;
}

AthrillJitOperand athrill_jit_imm(sint64 value)
{
	return operand(ATHRILL_JIT_OPERAND_IMM, 0U, value);
}

AthrillJitOperand athrill_jit_core(uint32 offset, uint8 width)
{
	return operand(ATHRILL_JIT_OPERAND_CORE, width, (sint64)offset);
}

AthrillJitOperand athrill_jit_ctx(uint32 offset, uint8 width)
{
	return operand(ATHRILL_JIT_OPERAND_CTX, width, (sint64)offset);
}

AthrillJitOperand athrill_jit_abs(const void *address, uint8 width)
{
	return operand(ATHRILL_JIT_OPERAND_ABS, width, (sint64)(uintptr_t)address);
}

AthrillJitOperand athrill_jit_done(void)
{
	return operand(ATHRILL_JIT_OPERAND_DONE, 8U, 0);
}

AthrillJitOperand athrill_jit_result(void)
{
	return operand(ATHRILL_JIT_OPERAND_RESULT, 8U, 0);
}

static std_bool valid_width(uint8 width)
{
	return (width == 4U) || (width == 8U);
}

static std_bool is_destination(AthrillJitOperand op)
{
	if (op.kind == ATHRILL_JIT_OPERAND_CORE) {
		return TRUE;
	}
	if (op.kind == ATHRILL_JIT_OPERAND_CTX) {
		return TRUE;
	}
	if (op.kind == ATHRILL_JIT_OPERAND_ABS) {
		return TRUE;
	}
	return FALSE;
}

/* The sljit operand for op (fails the builder for an invalid one). */
static std_bool to_sljit(AthrillJitBuilder *b, AthrillJitOperand op, sljit_s32 *arg, sljit_sw *argw)
{
	if ((op.kind != ATHRILL_JIT_OPERAND_IMM) && !valid_width(op.width)) {
		fail(b, "operand width must be 4 or 8");
		return FALSE;
	}
	switch (op.kind) {
	case ATHRILL_JIT_OPERAND_IMM:
		*arg = SLJIT_IMM;
		*argw = (sljit_sw)op.value;
		return TRUE;
	case ATHRILL_JIT_OPERAND_CORE:
		*arg = SLJIT_MEM1(JIT_REG_CORE);
		*argw = (sljit_sw)op.value;
		return TRUE;
	case ATHRILL_JIT_OPERAND_CTX:
		*arg = SLJIT_MEM1(JIT_REG_CTX);
		*argw = (sljit_sw)op.value;
		return TRUE;
	case ATHRILL_JIT_OPERAND_ABS:
		*arg = SLJIT_MEM0();
		*argw = (sljit_sw)op.value;
		return TRUE;
	case ATHRILL_JIT_OPERAND_DONE:
		*arg = JIT_REG_DONE;
		*argw = 0;
		return TRUE;
	case ATHRILL_JIT_OPERAND_RESULT:
		*arg = JIT_REG_RESULT;
		*argw = 0;
		return TRUE;
	}
	fail(b, "unknown operand kind");
	return FALSE;
}

/* The width of an operation on a and c (IMM takes the other's width). */
static uint8 pair_width(AthrillJitOperand a, AthrillJitOperand c)
{
	if (a.kind != ATHRILL_JIT_OPERAND_IMM) {
		return a.width;
	}
	return c.width;
}

static sljit_s32 width_flag(uint8 width)
{
	if (width == 4U) {
		return SLJIT_32;
	}
	return 0;
}

/*
 * An 8-byte memory source in a 4-byte operation is read as its low half
 * (little-endian hosts: the same address). A 4-byte memory source in an
 * 8-byte operation must be widened first: it is loaded into reg.
 */
static std_bool load_widened(AthrillJitBuilder *b, AthrillJitOperand op, uint8 width, sljit_s32 reg,
			     sljit_s32 *arg, sljit_sw *argw)
{
	if (!to_sljit(b, op, arg, argw)) {
		return FALSE;
	}
	if ((width == 8U) && (op.kind != ATHRILL_JIT_OPERAND_IMM) && (op.width == 4U)) {
		sljit_emit_op1(b->c, SLJIT_MOV_U32, reg, 0, *arg, *argw);
		*arg = reg;
		*argw = 0;
	}
	return TRUE;
}

/* ---- builder ---- */

AthrillJitBuilder *athrill_jit_builder_create(void)
{
	AthrillJitBuilder *b = (AthrillJitBuilder *)calloc(1U, sizeof(*b));

	if (b == NULL) {
		set_error("out of memory");
		return NULL;
	}
	b->c = sljit_create_compiler(NULL);
	if (b->c == NULL) {
		set_error("sljit_create_compiler failed");
		free(b);
		return NULL;
	}
	/* uint64 block(void *core, void *ctx): core -> S0, ctx -> S1 */
	sljit_emit_enter(b->c, 0, SLJIT_ARGS2(W, P, P), 3, 4, 0);
	sljit_emit_op1(b->c, SLJIT_MOV, JIT_REG_DONE, 0, SLJIT_IMM, 0);
	sljit_emit_op1(b->c, SLJIT_MOV, JIT_REG_RESULT, 0, SLJIT_IMM, 0);
	b->exit_label = athrill_jit_label_new(b);
	if (b->exit_label == NULL) {
		athrill_jit_builder_abort(b);
		return NULL;
	}
	return b;
}

static void mov_common(AthrillJitBuilder *b, AthrillJitOperand dst, AthrillJitOperand src, std_bool sign_extend)
{
	sljit_s32 d;
	sljit_sw dw;
	sljit_s32 s;
	sljit_sw sw;
	sljit_s32 op;

	if (b->failed) {
		return;
	}
	if (!is_destination(dst)) {
		fail(b, "mov: the destination must be a CORE, CTX or ABS field");
		return;
	}
	if (!to_sljit(b, dst, &d, &dw) || !to_sljit(b, src, &s, &sw)) {
		return;
	}
	if (dst.width == 4U) {
		sljit_emit_op1(b->c, SLJIT_MOV32, d, dw, s, sw);
		return;
	}
	if ((src.kind != ATHRILL_JIT_OPERAND_IMM) && (src.width == 4U)) {
		/*
		 * Widen in a register: MOV_U32 / MOV_S32 with a memory destination
		 * would store only 4 bytes and leave the upper half as it was.
		 */
		op = sign_extend ? SLJIT_MOV_S32 : SLJIT_MOV_U32;
		sljit_emit_op1(b->c, op, SLJIT_R0, 0, s, sw);
		sljit_emit_op1(b->c, SLJIT_MOV, d, dw, SLJIT_R0, 0);
		return;
	}
	sljit_emit_op1(b->c, SLJIT_MOV, d, dw, s, sw);
}

void athrill_jit_mov(AthrillJitBuilder *b, AthrillJitOperand dst, AthrillJitOperand src)
{
	mov_common(b, dst, src, FALSE);
}

void athrill_jit_movs(AthrillJitBuilder *b, AthrillJitOperand dst, AthrillJitOperand src)
{
	mov_common(b, dst, src, TRUE);
}

static std_bool is_division(AthrillJitAluOp op)
{
	switch (op) {
	case ATHRILL_JIT_DIVU:
	case ATHRILL_JIT_DIVS:
	case ATHRILL_JIT_REMU:
	case ATHRILL_JIT_REMS:
		return TRUE;
	default:
		return FALSE;
	}
}

static std_bool is_signed_division(AthrillJitAluOp op)
{
	if (op == ATHRILL_JIT_DIVS) {
		return TRUE;
	}
	if (op == ATHRILL_JIT_REMS) {
		return TRUE;
	}
	return FALSE;
}

static sljit_s32 sljit_op2_of(AthrillJitAluOp op)
{
	switch (op) {
	case ATHRILL_JIT_ADD:
		return SLJIT_ADD;
	case ATHRILL_JIT_SUB:
		return SLJIT_SUB;
	case ATHRILL_JIT_MUL:
		return SLJIT_MUL;
	case ATHRILL_JIT_AND:
		return SLJIT_AND;
	case ATHRILL_JIT_OR:
		return SLJIT_OR;
	case ATHRILL_JIT_XOR:
		return SLJIT_XOR;
	case ATHRILL_JIT_SHL:
		return SLJIT_MSHL;	/* masked count on every host */
	case ATHRILL_JIT_SHR:
		return SLJIT_MLSHR;
	case ATHRILL_JIT_SAR:
		return SLJIT_MASHR;
	default:
		return -1;
	}
}

/*
 * Division: R0 = a, R1 = c, then sljit's divide (R0 = quotient,
 * R1 = remainder). The target guarded the divisor; an immediate divisor
 * that could trap the host is refused here.
 */
static void emit_division(AthrillJitBuilder *b, AthrillJitAluOp op, uint8 width,
			  sljit_s32 d, sljit_sw dw, AthrillJitOperand a, AthrillJitOperand c)
{
	sljit_s32 s;
	sljit_sw sw;
	sljit_s32 div_op;
	sljit_s32 result_reg;

	if (c.kind == ATHRILL_JIT_OPERAND_IMM) {
		if (c.value == 0) {
			fail(b, "alu: division by an immediate 0");
			return;
		}
		if (is_signed_division(op) && (c.value == -1)) {
			fail(b, "alu: signed division by an immediate -1 (MIN / -1 traps the host)");
			return;
		}
	}
	if (!load_widened(b, a, width, SLJIT_R0, &s, &sw)) {
		return;
	}
	sljit_emit_op1(b->c, (width == 4U) ? SLJIT_MOV32 : SLJIT_MOV, SLJIT_R0, 0, s, sw);
	if (!load_widened(b, c, width, SLJIT_R1, &s, &sw)) {
		return;
	}
	sljit_emit_op1(b->c, (width == 4U) ? SLJIT_MOV32 : SLJIT_MOV, SLJIT_R1, 0, s, sw);

	if (is_signed_division(op)) {
		div_op = SLJIT_DIVMOD_SW;
	} else {
		div_op = SLJIT_DIVMOD_UW;
	}
	sljit_emit_op0(b->c, div_op | width_flag(width));

	if ((op == ATHRILL_JIT_DIVU) || (op == ATHRILL_JIT_DIVS)) {
		result_reg = SLJIT_R0;
	} else {
		result_reg = SLJIT_R1;
	}
	sljit_emit_op1(b->c, (width == 4U) ? SLJIT_MOV32 : SLJIT_MOV, d, dw, result_reg, 0);
}

void athrill_jit_alu(AthrillJitBuilder *b, AthrillJitAluOp op,
		     AthrillJitOperand dst, AthrillJitOperand a, AthrillJitOperand c)
{
	sljit_s32 d;
	sljit_sw dw;
	sljit_s32 s1;
	sljit_sw s1w;
	sljit_s32 s2;
	sljit_sw s2w;
	sljit_s32 sop;
	uint8 width;

	if (b->failed) {
		return;
	}
	if (!is_destination(dst)) {
		fail(b, "alu: the destination must be a CORE, CTX or ABS field");
		return;
	}
	if (!to_sljit(b, dst, &d, &dw)) {
		return;
	}
	width = dst.width;
	if (is_division(op)) {
		emit_division(b, op, width, d, dw, a, c);
		return;
	}
	sop = sljit_op2_of(op);
	if (sop < 0) {
		fail(b, "alu: unknown operation");
		return;
	}
	if (!load_widened(b, a, width, SLJIT_R0, &s1, &s1w)) {
		return;
	}
	if (!load_widened(b, c, width, SLJIT_R1, &s2, &s2w)) {
		return;
	}
	sljit_emit_op2(b->c, sop | width_flag(width), d, dw, s1, s1w, s2, s2w);
}

/* The sljit move of size bytes (sign: the signed load forms). -1: invalid size. */
static sljit_s32 host_move_of(uint8 size, std_bool sign_extend)
{
	switch (size) {
	case 1U:
		return sign_extend ? SLJIT_MOV_S8 : SLJIT_MOV_U8;
	case 2U:
		return sign_extend ? SLJIT_MOV_S16 : SLJIT_MOV_U16;
	case 4U:
		return sign_extend ? SLJIT_MOV_S32 : SLJIT_MOV_U32;
	case 8U:
		return SLJIT_MOV;
	default:
		return -1;
	}
}

/* R0 = addr + bias (both 8 bytes). */
static std_bool host_address(AthrillJitBuilder *b, AthrillJitOperand addr, AthrillJitOperand bias)
{
	sljit_s32 s1;
	sljit_sw s1w;
	sljit_s32 s2;
	sljit_sw s2w;

	if ((addr.kind != ATHRILL_JIT_OPERAND_IMM) && (addr.width != 8U)) {
		fail(b, "host access: the address must be 8 bytes");
		return FALSE;
	}
	if ((bias.kind != ATHRILL_JIT_OPERAND_IMM) && (bias.width != 8U)) {
		fail(b, "host access: the bias must be 8 bytes");
		return FALSE;
	}
	if (!to_sljit(b, addr, &s1, &s1w) || !to_sljit(b, bias, &s2, &s2w)) {
		return FALSE;
	}
	sljit_emit_op2(b->c, SLJIT_ADD, SLJIT_R0, 0, s1, s1w, s2, s2w);
	return TRUE;
}

void athrill_jit_load_host(AthrillJitBuilder *b, AthrillJitOperand dst, AthrillJitOperand addr,
			   AthrillJitOperand bias, uint8 size, std_bool sign_extend)
{
	sljit_s32 d;
	sljit_sw dw;
	sljit_s32 move = host_move_of(size, sign_extend);

	if (b->failed) {
		return;
	}
	if (move < 0) {
		fail(b, "load_host: size must be 1, 2, 4 or 8");
		return;
	}
	if (!is_destination(dst) || (dst.width != 8U)) {
		fail(b, "load_host: the destination must be an 8-byte CORE, CTX or ABS field");
		return;
	}
	if (!to_sljit(b, dst, &d, &dw) || !host_address(b, addr, bias)) {
		return;
	}
	sljit_emit_op1(b->c, move, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), 0);
	sljit_emit_op1(b->c, SLJIT_MOV, d, dw, SLJIT_R1, 0);
}

void athrill_jit_store_host(AthrillJitBuilder *b, AthrillJitOperand src, AthrillJitOperand addr,
			    AthrillJitOperand bias, uint8 size)
{
	sljit_s32 s;
	sljit_sw sw;
	sljit_s32 move = host_move_of(size, FALSE);

	if (b->failed) {
		return;
	}
	if (move < 0) {
		fail(b, "store_host: size must be 1, 2, 4 or 8");
		return;
	}
	if ((src.kind != ATHRILL_JIT_OPERAND_IMM) && (src.width < size)) {
		fail(b, "store_host: the source is narrower than size");
		return;
	}
	if (!to_sljit(b, src, &s, &sw)) {
		return;
	}
	/* the value first: R0 is the address */
	sljit_emit_op1(b->c, (size == 8U) ? SLJIT_MOV : SLJIT_MOV32, SLJIT_R1, 0, s, sw);
	if (!host_address(b, addr, bias)) {
		return;
	}
	sljit_emit_op1(b->c, move, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_R1, 0);
}

void athrill_jit_count(AthrillJitBuilder *b, uint32 n)
{
	if (b->failed) {
		return;
	}
	sljit_emit_op2(b->c, SLJIT_ADD, JIT_REG_DONE, 0, JIT_REG_DONE, 0, SLJIT_IMM, (sljit_sw)n);
}

void athrill_jit_call(AthrillJitBuilder *b, AthrillJitCallFunc func, const void *arg)
{
	if (b->failed) {
		return;
	}
	if (func == NULL) {
		fail(b, "call: NULL function");
		return;
	}
	sljit_emit_op1(b->c, SLJIT_MOV_P, SLJIT_R0, 0, JIT_REG_CORE, 0);
	sljit_emit_op1(b->c, SLJIT_MOV_P, SLJIT_R1, 0, JIT_REG_CTX, 0);
	sljit_emit_op1(b->c, SLJIT_MOV_P, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)(uintptr_t)arg);
	sljit_emit_icall(b->c, SLJIT_CALL, SLJIT_ARGS3(32, P, P, P), SLJIT_IMM, SLJIT_FUNC_ADDR(func));
	sljit_emit_op1(b->c, SLJIT_MOV_S32, JIT_REG_RESULT, 0, SLJIT_R0, 0);
}

/* ---- labels and jumps ---- */

AthrillJitLabel *athrill_jit_label_new(AthrillJitBuilder *b)
{
	AthrillJitLabel *label = (AthrillJitLabel *)calloc(1U, sizeof(*label));

	if (label == NULL) {
		fail(b, "out of memory");
		return NULL;
	}
	label->next_in_builder = b->labels;
	b->labels = label;
	return label;
}

static void label_add_pending(AthrillJitBuilder *b, AthrillJitLabel *label, struct sljit_jump *jump)
{
	struct sljit_jump **grown;
	uint32 cap;

	if (label->pending_num == label->pending_cap) {
		cap = (label->pending_cap == 0U) ? 4U : label->pending_cap * 2U;
		grown = (struct sljit_jump **)realloc(label->pending, cap * sizeof(*grown));
		if (grown == NULL) {
			fail(b, "out of memory");
			return;
		}
		label->pending = grown;
		label->pending_cap = cap;
	}
	label->pending[label->pending_num] = jump;
	label->pending_num++;
}

static void bind_jump(AthrillJitBuilder *b, AthrillJitLabel *label, struct sljit_jump *jump)
{
	if (jump == NULL) {
		fail(b, "sljit could not emit a jump");
		return;
	}
	if (label->placed != NULL) {
		sljit_set_label(jump, label->placed);
	} else {
		label_add_pending(b, label, jump);
	}
}

void athrill_jit_label_place(AthrillJitBuilder *b, AthrillJitLabel *label)
{
	uint32 i;

	if (b->failed) {
		return;
	}
	if ((label == NULL) || (label->placed != NULL)) {
		fail(b, "label: NULL or placed twice");
		return;
	}
	label->placed = sljit_emit_label(b->c);
	for (i = 0U; i < label->pending_num; i++) {
		sljit_set_label(label->pending[i], label->placed);
	}
	label->pending_num = 0U;
}

void athrill_jit_jump(AthrillJitBuilder *b, AthrillJitLabel *label)
{
	if (b->failed) {
		return;
	}
	if (label == NULL) {
		fail(b, "jump: NULL label");
		return;
	}
	bind_jump(b, label, sljit_emit_jump(b->c, SLJIT_JUMP));
}

static sljit_s32 sljit_cond_of(AthrillJitCond cond)
{
	switch (cond) {
	case ATHRILL_JIT_EQ:
		return SLJIT_EQUAL;
	case ATHRILL_JIT_NE:
		return SLJIT_NOT_EQUAL;
	case ATHRILL_JIT_LTU:
		return SLJIT_LESS;
	case ATHRILL_JIT_GEU:
		return SLJIT_GREATER_EQUAL;
	case ATHRILL_JIT_GTU:
		return SLJIT_GREATER;
	case ATHRILL_JIT_LEU:
		return SLJIT_LESS_EQUAL;
	case ATHRILL_JIT_LTS:
		return SLJIT_SIG_LESS;
	case ATHRILL_JIT_GES:
		return SLJIT_SIG_GREATER_EQUAL;
	case ATHRILL_JIT_GTS:
		return SLJIT_SIG_GREATER;
	case ATHRILL_JIT_LES:
		return SLJIT_SIG_LESS_EQUAL;
	}
	return -1;
}

static struct sljit_jump *emit_compare(AthrillJitBuilder *b, AthrillJitCond cond,
				       AthrillJitOperand a, AthrillJitOperand c)
{
	sljit_s32 s1;
	sljit_sw s1w;
	sljit_s32 s2;
	sljit_sw s2w;
	sljit_s32 type = sljit_cond_of(cond);
	uint8 width = pair_width(a, c);

	if (type < 0) {
		fail(b, "jump_if: unknown condition");
		return NULL;
	}
	if (!valid_width(width)) {
		fail(b, "jump_if: two immediates (no width)");
		return NULL;
	}
	if (!load_widened(b, a, width, SLJIT_R0, &s1, &s1w)) {
		return NULL;
	}
	if (!load_widened(b, c, width, SLJIT_R1, &s2, &s2w)) {
		return NULL;
	}
	return sljit_emit_cmp(b->c, type | width_flag(width), s1, s1w, s2, s2w);
}

void athrill_jit_jump_if(AthrillJitBuilder *b, AthrillJitCond cond,
			 AthrillJitOperand a, AthrillJitOperand c, AthrillJitLabel *label)
{
	struct sljit_jump *jump;

	if (b->failed) {
		return;
	}
	if (label == NULL) {
		fail(b, "jump_if: NULL label");
		return;
	}
	jump = emit_compare(b, cond, a, c);
	if (b->failed) {
		return;
	}
	bind_jump(b, label, jump);
}

void athrill_jit_exit(AthrillJitBuilder *b)
{
	athrill_jit_jump(b, b->exit_label);
}

void athrill_jit_exit_if(AthrillJitBuilder *b, AthrillJitCond cond,
			 AthrillJitOperand a, AthrillJitOperand c)
{
	athrill_jit_jump_if(b, cond, a, c, b->exit_label);
}

/* ---- finish ---- */

static void free_labels(AthrillJitBuilder *b)
{
	AthrillJitLabel *label = b->labels;
	AthrillJitLabel *next;

	while (label != NULL) {
		next = label->next_in_builder;
		free(label->pending);
		free(label);
		label = next;
	}
	b->labels = NULL;
}

static std_bool labels_complete(AthrillJitBuilder *b)
{
	AthrillJitLabel *label;

	for (label = b->labels; label != NULL; label = label->next_in_builder) {
		if (label->placed == NULL) {
			return FALSE;
		}
	}
	return TRUE;
}

void athrill_jit_builder_abort(AthrillJitBuilder *b)
{
	if (b == NULL) {
		return;
	}
	free_labels(b);
	sljit_free_compiler(b->c);
	free(b);
}

AthrillJitCode *athrill_jit_builder_finish(AthrillJitBuilder *b)
{
	AthrillJitCode *code;
	void *entry;
	sljit_s32 err;

	if (b == NULL) {
		return NULL;
	}
	/* A block that falls off its end exits like athrill_jit_exit(). */
	athrill_jit_label_place(b, b->exit_label);
	if (!b->failed && !labels_complete(b)) {
		fail(b, "a label was used but never placed");
	}
	if (b->failed) {
		athrill_jit_builder_abort(b);
		return NULL;
	}
	sljit_emit_return(b->c, SLJIT_MOV, JIT_REG_DONE, 0);
	err = sljit_get_compiler_error(b->c);
	if (err != SLJIT_SUCCESS) {
		set_error("sljit error %d while building", (int)err);
		athrill_jit_builder_abort(b);
		return NULL;
	}
	entry = sljit_generate_code(b->c, 0, NULL);
	if (entry == NULL) {
		set_error("sljit_generate_code failed (%d)", (int)sljit_get_compiler_error(b->c));
		athrill_jit_builder_abort(b);
		return NULL;
	}
	code = (AthrillJitCode *)malloc(sizeof(*code));
	if (code == NULL) {
		sljit_free_code(entry, NULL);
		set_error("out of memory");
		athrill_jit_builder_abort(b);
		return NULL;
	}
	code->entry = entry;
	code->size = (uint32)sljit_get_generated_code_size(b->c);
	athrill_jit_builder_abort(b);
	return code;
}

/* ---- running ---- */

uint64 athrill_jit_code_run(const AthrillJitCode *code, void *core, void *ctx)
{
	JitEntryFunc fn = (JitEntryFunc)code->entry;

	return (uint64)fn(core, ctx);
}

uint32 athrill_jit_code_size(const AthrillJitCode *code)
{
	return code->size;
}

const void *athrill_jit_code_entry(const AthrillJitCode *code)
{
	return code->entry;
}

void athrill_jit_code_free(AthrillJitCode *code)
{
	if (code == NULL) {
		return;
	}
	sljit_free_code(code->entry, NULL);
	free(code);
}

/* ---- block cache ---- */

typedef struct {
	std_bool used;
	AthrillAddrType pc;
	uint32 key;
	uint64 generation;
	AthrillJitCode *code;	/* NULL: the slot is kept for (pc, key) but holds no block */
} JitCacheSlot;

struct AthrillJitCache {
	JitCacheSlot *slots;
	uint32 capacity;	/* a power of 2 */
	uint32 used_slots;
	AthrillJitCacheStats stats;
};

AthrillJitCache *athrill_jit_cache_create(uint32 capacity)
{
	AthrillJitCache *cache;
	uint32 cap = 16U;

	while (cap < capacity) {
		cap *= 2U;
	}
	cache = (AthrillJitCache *)calloc(1U, sizeof(*cache));
	if (cache == NULL) {
		return NULL;
	}
	/* twice the blocks, so the table never fills */
	cache->slots = (JitCacheSlot *)calloc((size_t)cap * 2U, sizeof(JitCacheSlot));
	if (cache->slots == NULL) {
		free(cache);
		return NULL;
	}
	cache->capacity = cap;
	return cache;
}

static uint32 slot_count(const AthrillJitCache *cache)
{
	return cache->capacity * 2U;
}

static uint32 slot_hash(const AthrillJitCache *cache, AthrillAddrType pc, uint32 key)
{
	uint64 h = ((uint64)pc ^ ((uint64)key << 48)) * 0x9E3779B97F4A7C15ULL;

	return (uint32)(h >> 32) & (slot_count(cache) - 1U);
}

void athrill_jit_cache_flush(AthrillJitCache *cache)
{
	uint32 i;

	for (i = 0U; i < slot_count(cache); i++) {
		athrill_jit_code_free(cache->slots[i].code);
	}
	memset(cache->slots, 0, (size_t)slot_count(cache) * sizeof(JitCacheSlot));
	cache->used_slots = 0U;
	cache->stats.blocks = 0U;
	cache->stats.code_bytes = 0U;
}

void athrill_jit_cache_destroy(AthrillJitCache *cache)
{
	if (cache == NULL) {
		return;
	}
	athrill_jit_cache_flush(cache);
	free(cache->slots);
	free(cache);
}

/* The slot of (pc, key), or the empty slot where it would go. */
static JitCacheSlot *find_slot(AthrillJitCache *cache, AthrillAddrType pc, uint32 key)
{
	uint32 mask = slot_count(cache) - 1U;
	uint32 i = slot_hash(cache, pc, key);
	JitCacheSlot *slot;

	for (;;) {
		slot = &cache->slots[i];
		if (!slot->used) {
			return slot;
		}
		if ((slot->pc == pc) && (slot->key == key)) {
			return slot;
		}
		i = (i + 1U) & mask;
	}
}

static void drop_block(AthrillJitCache *cache, JitCacheSlot *slot)
{
	if (slot->code == NULL) {
		return;
	}
	cache->stats.code_bytes -= slot->code->size;
	cache->stats.blocks--;
	athrill_jit_code_free(slot->code);
	slot->code = NULL;
}

const AthrillJitCode *athrill_jit_cache_lookup(AthrillJitCache *cache,
					       AthrillAddrType pc, uint32 key, uint64 generation)
{
	JitCacheSlot *slot = find_slot(cache, pc, key);

	cache->stats.lookups++;
	if (!slot->used || (slot->code == NULL)) {
		return NULL;
	}
	if (slot->generation != generation) {
		cache->stats.stale++;
		drop_block(cache, slot);
		return NULL;
	}
	cache->stats.hits++;
	return slot->code;
}

void athrill_jit_cache_insert(AthrillJitCache *cache, AthrillAddrType pc, uint32 key,
			      uint64 generation, AthrillJitCode *code)
{
	JitCacheSlot *slot;

	if (code == NULL) {
		return;
	}
	slot = find_slot(cache, pc, key);
	if (!slot->used && (cache->used_slots >= cache->capacity)) {
		cache->stats.flushes++;
		athrill_jit_cache_flush(cache);
		slot = find_slot(cache, pc, key);
	}
	if (slot->used) {
		drop_block(cache, slot);
	} else {
		slot->used = TRUE;
		slot->pc = pc;
		slot->key = key;
		cache->used_slots++;
	}
	slot->generation = generation;
	slot->code = code;
	cache->stats.inserts++;
	cache->stats.blocks++;
	cache->stats.code_bytes += code->size;
}

void athrill_jit_cache_get_stats(const AthrillJitCache *cache, AthrillJitCacheStats *stats)
{
	*stats = cache->stats;
}
