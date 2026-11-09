/*
 * Unit tests of the target-independent JIT (src/main/jit/athrill_jit.h).
 *
 * Every result is compared with an independent C reference, never with
 * the JIT itself:
 *   1. alu: every operation, 32 and 64 bit, every operand kind, random and
 *      boundary values; shift counts masked as specified.
 *   2. mov / movs: widening (zero / sign) and narrowing.
 *   3. conditions: every condition, 32 and 64 bit, at the signed and
 *      unsigned boundaries.
 *   4. division: the RISC-V rules (x / 0 = -1, x % 0 = x, MIN / -1 = MIN,
 *      MIN % -1 = 0) written with guards; on an x86 host a missing guard
 *      would kill the test.
 *   5. a toy target: an interpreter and a JIT (native ADDI/ADD/SUB/BNE,
 *      call-threaded for the rest) run the same programs under the same
 *      clock budgets; the whole CPU state, clock and executed counts must
 *      be identical.
 *   6. a trap in the middle of a block: the block stops at the faulting
 *      instruction with its pc and clock, as the interpreter does.
 *   7. the cache: hits, stale generations, keys, full-cache flush, stats.
 *   8. invalid descriptions are refused (no code, an error message).
 */
#include "athrill_jit.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static int failures;
static int checks;

#define CHECK(cond, ...) \
	do { \
		checks++; \
		if (!(cond)) { \
			failures++; \
			if (failures <= 20) { \
				printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
				printf(__VA_ARGS__); \
				printf("\n"); \
			} \
		} \
	} while (0)

static uint64 rng_state = 0x0123456789ABCDEFULL;

static uint64 rnd(void)
{
	/* xorshift64*: fixed seed, the same numbers on every host */
	rng_state ^= rng_state >> 12;
	rng_state ^= rng_state << 25;
	rng_state ^= rng_state >> 27;
	return rng_state * 0x2545F4914F6CDD1DULL;
}

static const uint64 boundary64[] = {
	0ULL, 1ULL, 2ULL, 31ULL, 32ULL, 33ULL, 63ULL, 64ULL, 65ULL,
	0x7FFFFFFFULL, 0x80000000ULL, 0xFFFFFFFFULL, 0x100000000ULL,
	0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFEULL, 0x123456789ABCDEF0ULL,
};
#define BOUNDARY_NUM (sizeof(boundary64) / sizeof(boundary64[0]))

static uint64 sample(int i)
{
	if (i < (int)BOUNDARY_NUM) {
		return boundary64[i];
	}
	return rnd();
}

/* ------------------------------------------------------------------ */
/* 1. alu                                                              */

typedef struct {
	uint64 a;
	uint64 c;
	uint64 dst;
	uint32 a32;
	uint32 c32;
	uint32 dst32;
} AluCore;

typedef struct {
	uint64 c;
	uint32 c32;
} AluCtx;

static uint64 abs_var64;
static uint32 abs_var32;

static uint64 ref_alu(AthrillJitAluOp op, uint64 a, uint64 c, int width)
{
	uint64 mask = (width == 4) ? 0xFFFFFFFFULL : 0xFFFFFFFFFFFFFFFFULL;
	unsigned shift = (unsigned)(c & (uint64)(width * 8 - 1));
	uint64 r;

	a &= mask;
	c &= mask;
	switch (op) {
	case ATHRILL_JIT_ADD:
		r = a + c;
		break;
	case ATHRILL_JIT_SUB:
		r = a - c;
		break;
	case ATHRILL_JIT_MUL:
		r = a * c;
		break;
	case ATHRILL_JIT_AND:
		r = a & c;
		break;
	case ATHRILL_JIT_OR:
		r = a | c;
		break;
	case ATHRILL_JIT_XOR:
		r = a ^ c;
		break;
	case ATHRILL_JIT_SHL:
		r = a << shift;
		break;
	case ATHRILL_JIT_SHR:
		r = a >> shift;
		break;
	case ATHRILL_JIT_SAR:
		if (width == 4) {
			r = (uint64)(uint32)((sint32)(uint32)a >> shift);
		} else {
			r = (uint64)((sint64)a >> shift);
		}
		break;
	default:
		r = 0;
		break;
	}
	return r & mask;
}

static const char *alu_name[] = {
	"ADD", "SUB", "MUL", "AND", "OR", "XOR", "SHL", "SHR", "SAR",
};

/* The second source as each operand kind: 0 core, 1 ctx, 2 abs, 3 imm. */
static AthrillJitOperand second_source(int kind, int width, uint64 value)
{
	if (kind == 0) {
		return (width == 4) ? athrill_jit_core(offsetof(AluCore, c32), 4) : athrill_jit_core(offsetof(AluCore, c), 8);
	}
	if (kind == 1) {
		return (width == 4) ? athrill_jit_ctx(offsetof(AluCtx, c32), 4) : athrill_jit_ctx(offsetof(AluCtx, c), 8);
	}
	if (kind == 2) {
		return (width == 4) ? athrill_jit_abs(&abs_var32, 4) : athrill_jit_abs(&abs_var64, 8);
	}
	return athrill_jit_imm((sint64)value);
}

static void test_alu(void)
{
	int op;
	int width;
	int kind;
	int i;
	AthrillJitBuilder *b;
	AthrillJitCode *code;
	AluCore core;
	AluCtx ctx;
	uint64 a;
	uint64 c;
	uint64 got;
	uint64 want;

	for (op = ATHRILL_JIT_ADD; op <= ATHRILL_JIT_SAR; op++) {
		for (width = 4; width <= 8; width += 4) {
			for (kind = 0; kind < 4; kind++) {
				/* An immediate is fixed in the code: one block per value. */
				int cases = (kind == 3) ? 24 : 400;
				AthrillJitCode *shared = NULL;

				if (kind != 3) {
					b = athrill_jit_builder_create();
					athrill_jit_alu(b, (AthrillJitAluOp)op,
						(width == 4) ? athrill_jit_core(offsetof(AluCore, dst32), 4) : athrill_jit_core(offsetof(AluCore, dst), 8),
						(width == 4) ? athrill_jit_core(offsetof(AluCore, a32), 4) : athrill_jit_core(offsetof(AluCore, a), 8),
						second_source(kind, width, 0));
					shared = athrill_jit_builder_finish(b);
					CHECK(shared != NULL, "alu %s/%d kind %d: no code (%s)", alu_name[op], width, kind, athrill_jit_last_error());
					if (shared == NULL) {
						continue;
					}
				}
				for (i = 0; i < cases; i++) {
					a = sample(i);
					c = sample((i * 7 + 3) % ((int)BOUNDARY_NUM + 22));
					if (kind == 3) {
						c = sample(i);
						a = rnd();
						b = athrill_jit_builder_create();
						athrill_jit_alu(b, (AthrillJitAluOp)op,
							(width == 4) ? athrill_jit_core(offsetof(AluCore, dst32), 4) : athrill_jit_core(offsetof(AluCore, dst), 8),
							(width == 4) ? athrill_jit_core(offsetof(AluCore, a32), 4) : athrill_jit_core(offsetof(AluCore, a), 8),
							second_source(kind, width, c));
						code = athrill_jit_builder_finish(b);
						CHECK(code != NULL, "alu imm: no code (%s)", athrill_jit_last_error());
						if (code == NULL) {
							continue;
						}
					} else {
						code = shared;
					}
					memset(&core, 0xA5, sizeof(core));
					core.a = a;
					core.a32 = (uint32)a;
					core.c = c;
					core.c32 = (uint32)c;
					ctx.c = c;
					ctx.c32 = (uint32)c;
					abs_var64 = c;
					abs_var32 = (uint32)c;
					(void)athrill_jit_code_run(code, &core, &ctx);
					got = (width == 4) ? core.dst32 : core.dst;
					want = ref_alu((AthrillJitAluOp)op, a, c, width);
					CHECK(got == want, "alu %s/%d kind %d: a=%016llx c=%016llx got %016llx want %016llx",
					      alu_name[op], width, kind, (unsigned long long)a, (unsigned long long)c,
					      (unsigned long long)got, (unsigned long long)want);
					if (width == 4) {
						CHECK(core.dst == 0xA5A5A5A5A5A5A5A5ULL, "alu 32-bit wrote the 64-bit field");
					}
					if (kind == 3) {
						athrill_jit_code_free(code);
					}
				}
				athrill_jit_code_free(shared);
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/* 2. mov / movs                                                       */

typedef struct {
	uint32 s32;
	uint64 s64;
	uint64 d64;
	uint32 d32;
} MovCore;

static void test_mov(void)
{
	int i;
	MovCore core;
	AthrillJitBuilder *b;
	AthrillJitCode *zext;
	AthrillJitCode *sext;
	AthrillJitCode *narrow;
	uint64 v;

	b = athrill_jit_builder_create();
	athrill_jit_mov(b, athrill_jit_core(offsetof(MovCore, d64), 8), athrill_jit_core(offsetof(MovCore, s32), 4));
	zext = athrill_jit_builder_finish(b);
	b = athrill_jit_builder_create();
	athrill_jit_movs(b, athrill_jit_core(offsetof(MovCore, d64), 8), athrill_jit_core(offsetof(MovCore, s32), 4));
	sext = athrill_jit_builder_finish(b);
	b = athrill_jit_builder_create();
	athrill_jit_mov(b, athrill_jit_core(offsetof(MovCore, d32), 4), athrill_jit_core(offsetof(MovCore, s64), 8));
	narrow = athrill_jit_builder_finish(b);
	CHECK((zext != NULL) && (sext != NULL) && (narrow != NULL), "mov: no code (%s)", athrill_jit_last_error());
	if ((zext == NULL) || (sext == NULL) || (narrow == NULL)) {
		return;
	}
	for (i = 0; i < 200; i++) {
		v = sample(i);
		memset(&core, 0x5A, sizeof(core));
		core.s32 = (uint32)v;
		core.s64 = v;
		(void)athrill_jit_code_run(zext, &core, NULL);
		CHECK(core.d64 == (uint64)(uint32)v, "mov zero-extend %08x -> %016llx", (unsigned)(uint32)v, (unsigned long long)core.d64);
		(void)athrill_jit_code_run(sext, &core, NULL);
		CHECK(core.d64 == (uint64)(sint64)(sint32)(uint32)v, "movs sign-extend %08x -> %016llx", (unsigned)(uint32)v, (unsigned long long)core.d64);
		(void)athrill_jit_code_run(narrow, &core, NULL);
		CHECK(core.d32 == (uint32)v, "mov narrow %016llx -> %08x", (unsigned long long)v, (unsigned)core.d32);
	}
	athrill_jit_code_free(zext);
	athrill_jit_code_free(sext);
	athrill_jit_code_free(narrow);
}

/* ------------------------------------------------------------------ */
/* 3. conditions                                                       */

typedef struct {
	uint64 a;
	uint64 c;
	uint32 a32;
	uint32 c32;
	uint32 taken;
} CondCore;

static int ref_cond(AthrillJitCond cond, uint64 a, uint64 c, int width)
{
	sint64 sa;
	sint64 sc;

	if (width == 4) {
		a &= 0xFFFFFFFFULL;
		c &= 0xFFFFFFFFULL;
		sa = (sint32)(uint32)a;
		sc = (sint32)(uint32)c;
	} else {
		sa = (sint64)a;
		sc = (sint64)c;
	}
	switch (cond) {
	case ATHRILL_JIT_EQ: return a == c;
	case ATHRILL_JIT_NE: return a != c;
	case ATHRILL_JIT_LTU: return a < c;
	case ATHRILL_JIT_GEU: return a >= c;
	case ATHRILL_JIT_GTU: return a > c;
	case ATHRILL_JIT_LEU: return a <= c;
	case ATHRILL_JIT_LTS: return sa < sc;
	case ATHRILL_JIT_GES: return sa >= sc;
	case ATHRILL_JIT_GTS: return sa > sc;
	case ATHRILL_JIT_LES: return sa <= sc;
	}
	return -1;
}

static void test_conditions(void)
{
	int cond;
	int width;
	int i;
	int j;
	AthrillJitBuilder *b;
	AthrillJitCode *code;
	AthrillJitLabel *yes;
	AthrillJitLabel *end;
	CondCore core;

	for (cond = ATHRILL_JIT_EQ; cond <= ATHRILL_JIT_LES; cond++) {
		for (width = 4; width <= 8; width += 4) {
			b = athrill_jit_builder_create();
			yes = athrill_jit_label_new(b);
			end = athrill_jit_label_new(b);
			athrill_jit_jump_if(b, (AthrillJitCond)cond,
				(width == 4) ? athrill_jit_core(offsetof(CondCore, a32), 4) : athrill_jit_core(offsetof(CondCore, a), 8),
				(width == 4) ? athrill_jit_core(offsetof(CondCore, c32), 4) : athrill_jit_core(offsetof(CondCore, c), 8),
				yes);
			athrill_jit_mov(b, athrill_jit_core(offsetof(CondCore, taken), 4), athrill_jit_imm(0));
			athrill_jit_jump(b, end);
			athrill_jit_label_place(b, yes);
			athrill_jit_mov(b, athrill_jit_core(offsetof(CondCore, taken), 4), athrill_jit_imm(1));
			athrill_jit_label_place(b, end);
			code = athrill_jit_builder_finish(b);
			CHECK(code != NULL, "cond %d/%d: no code (%s)", cond, width, athrill_jit_last_error());
			if (code == NULL) {
				continue;
			}
			for (i = 0; i < (int)BOUNDARY_NUM + 20; i++) {
				for (j = 0; j < (int)BOUNDARY_NUM + 20; j++) {
					uint64 a = sample(i);
					uint64 c = (i == j) ? a : sample(j);

					core.a = a;
					core.c = c;
					core.a32 = (uint32)a;
					core.c32 = (uint32)c;
					core.taken = 7U;
					(void)athrill_jit_code_run(code, &core, NULL);
					CHECK((int)core.taken == ref_cond((AthrillJitCond)cond, a, c, width),
					      "cond %d/%d a=%016llx c=%016llx got %u", cond, width,
					      (unsigned long long)a, (unsigned long long)c, core.taken);
				}
			}
			athrill_jit_code_free(code);
		}
	}
}

/* exit and exit_if: what runs before and after, and the done counter */
typedef struct {
	uint64 a;
	uint64 c;
	uint32 a32;
	uint32 c32;
	uint32 before;
	uint32 after;
} ExitCore;

static void test_exit_if(void)
{
	int cond;
	int width;
	int i;
	int j;
	AthrillJitBuilder *b;
	AthrillJitCode *code;
	ExitCore core;
	uint64 done;
	int want_exit;

	for (cond = ATHRILL_JIT_EQ; cond <= ATHRILL_JIT_LES; cond++) {
		for (width = 4; width <= 8; width += 4) {
			b = athrill_jit_builder_create();
			athrill_jit_count(b, 3U);
			athrill_jit_mov(b, athrill_jit_core(offsetof(ExitCore, before), 4), athrill_jit_imm(1));
			athrill_jit_exit_if(b, (AthrillJitCond)cond,
				(width == 4) ? athrill_jit_core(offsetof(ExitCore, a32), 4) : athrill_jit_core(offsetof(ExitCore, a), 8),
				(width == 4) ? athrill_jit_core(offsetof(ExitCore, c32), 4) : athrill_jit_core(offsetof(ExitCore, c), 8));
			athrill_jit_count(b, 4U);
			athrill_jit_mov(b, athrill_jit_core(offsetof(ExitCore, after), 4), athrill_jit_imm(1));
			athrill_jit_exit(b);
			/* never reached */
			athrill_jit_count(b, 100U);
			athrill_jit_mov(b, athrill_jit_core(offsetof(ExitCore, after), 4), athrill_jit_imm(9));
			code = athrill_jit_builder_finish(b);
			CHECK(code != NULL, "exit_if %d/%d: no code (%s)", cond, width, athrill_jit_last_error());
			if (code == NULL) {
				continue;
			}
			for (i = 0; i < (int)BOUNDARY_NUM + 6; i++) {
				for (j = 0; j < (int)BOUNDARY_NUM + 6; j++) {
					uint64 a = sample(i);
					uint64 c = (i == j) ? a : sample(j);

					core.a = a;
					core.c = c;
					core.a32 = (uint32)a;
					core.c32 = (uint32)c;
					core.before = 0U;
					core.after = 0U;
					done = athrill_jit_code_run(code, &core, NULL);
					want_exit = ref_cond((AthrillJitCond)cond, a, c, width);
					CHECK(core.before == 1U, "exit_if %d/%d: the operation before ran", cond, width);
					if (want_exit) {
						CHECK((done == 3U) && (core.after == 0U),
						      "exit_if %d/%d a=%016llx c=%016llx: should exit (done %llu, after %u)",
						      cond, width, (unsigned long long)a, (unsigned long long)c, (unsigned long long)done, core.after);
					} else {
						CHECK((done == 7U) && (core.after == 1U),
						      "exit_if %d/%d a=%016llx c=%016llx: should stay (done %llu, after %u)",
						      cond, width, (unsigned long long)a, (unsigned long long)c, (unsigned long long)done, core.after);
					}
				}
			}
			athrill_jit_code_free(code);
		}
	}
}

/* call: arguments, the RESULT operand, and fields written by the callee */
typedef struct {
	uint64 seen_ctx;
	uint64 value;
	sint64 result;
	uint32 result32;
	uint32 calls;
} CallCore;

typedef struct {
	uint64 tag;
} CallCtx;

static int call_probe(void *core_v, void *ctx_v, const void *arg)
{
	CallCore *core = (CallCore *)core_v;
	CallCtx *ctx = (CallCtx *)ctx_v;
	const sint32 *ret = (const sint32 *)arg;

	core->seen_ctx = ctx->tag;
	core->value = core->value * 3U + 1U;	/* read by the block afterwards */
	core->calls++;
	return *ret;
}

static void test_call(void)
{
	static const sint32 returns[] = { 0, 1, -1, 7, 0x7FFFFFFF, (sint32)0x80000000U, -12345 };
	unsigned k;
	AthrillJitBuilder *b;
	AthrillJitCode *code;
	CallCore core;
	CallCtx ctx;

	for (k = 0U; k < sizeof(returns) / sizeof(returns[0]); k++) {
		b = athrill_jit_builder_create();
		athrill_jit_call(b, call_probe, &returns[k]);
		athrill_jit_mov(b, athrill_jit_core(offsetof(CallCore, result), 8), athrill_jit_result());
		athrill_jit_mov(b, athrill_jit_core(offsetof(CallCore, result32), 4), athrill_jit_result());
		/* a native operation after the call sees the callee's write */
		athrill_jit_alu(b, ATHRILL_JIT_ADD, athrill_jit_core(offsetof(CallCore, value), 8),
				athrill_jit_core(offsetof(CallCore, value), 8), athrill_jit_imm(100));
		/* a second call: RESULT is replaced, ctx passed again */
		athrill_jit_call(b, call_probe, &returns[(k + 1U) % (sizeof(returns) / sizeof(returns[0]))]);
		athrill_jit_exit_if(b, ATHRILL_JIT_EQ, athrill_jit_result(), athrill_jit_imm(returns[k]));
		athrill_jit_count(b, 1U);
		code = athrill_jit_builder_finish(b);
		CHECK(code != NULL, "call: no code (%s)", athrill_jit_last_error());
		if (code == NULL) {
			continue;
		}
		memset(&core, 0, sizeof(core));
		core.value = 5U;
		ctx.tag = 0xC0FFEE00ULL + k;
		(void)athrill_jit_code_run(code, &core, &ctx);
		CHECK(core.result == (sint64)returns[k], "call: RESULT %lld want %d", (long long)core.result, returns[k]);
		CHECK(core.result32 == (uint32)returns[k], "call: RESULT (32) %08x want %08x", core.result32, (uint32)returns[k]);
		CHECK(core.seen_ctx == ctx.tag, "call: ctx passed (%llx)", (unsigned long long)core.seen_ctx);
		CHECK(core.calls == 2U, "call: two calls (%u)", core.calls);
		CHECK(core.value == ((5U * 3U + 1U) + 100U) * 3U + 1U, "call: field order (%llu)", (unsigned long long)core.value);
		athrill_jit_code_free(code);
	}
}

/* ------------------------------------------------------------------ */
/* 4. RISC-V division rules with guards                                */

typedef struct {
	uint64 a;
	uint64 c;
	uint64 q;
	uint32 a32;
	uint32 c32;
	uint32 q32;
} DivCore;

/* The RISC-V M extension's results (the reference). */
static uint64 riscv_div(AthrillJitAluOp op, uint64 a, uint64 c, int width)
{
	uint64 mask = (width == 4) ? 0xFFFFFFFFULL : 0xFFFFFFFFFFFFFFFFULL;
	uint64 min = (width == 4) ? 0x80000000ULL : 0x8000000000000000ULL;
	sint64 sa;
	sint64 sc;

	a &= mask;
	c &= mask;
	if (c == 0) {
		if ((op == ATHRILL_JIT_DIVU) || (op == ATHRILL_JIT_DIVS)) {
			return mask;	/* all ones */
		}
		return a;		/* the dividend */
	}
	if ((op == ATHRILL_JIT_DIVS) || (op == ATHRILL_JIT_REMS)) {
		if ((a == min) && (c == mask)) {
			return (op == ATHRILL_JIT_DIVS) ? min : 0;
		}
		if (width == 4) {
			sa = (sint32)(uint32)a;
			sc = (sint32)(uint32)c;
		} else {
			sa = (sint64)a;
			sc = (sint64)c;
		}
		if (op == ATHRILL_JIT_DIVS) {
			return (uint64)(sa / sc) & mask;
		}
		return (uint64)(sa % sc) & mask;
	}
	if (op == ATHRILL_JIT_DIVU) {
		return a / c;
	}
	return a % c;
}

/*
 * What a RISC-V target emits for div/rem: the two special cases first,
 * then the host division (which can no longer trap).
 */
static void emit_riscv_div(AthrillJitBuilder *b, AthrillJitAluOp op, int width,
			   AthrillJitOperand dst, AthrillJitOperand a, AthrillJitOperand c)
{
	AthrillJitLabel *by_zero = athrill_jit_label_new(b);
	AthrillJitLabel *overflow = athrill_jit_label_new(b);
	AthrillJitLabel *end = athrill_jit_label_new(b);
	sint64 min = (width == 4) ? (sint64)(sint32)0x80000000U : (sint64)0x8000000000000000ULL;
	int is_signed = (op == ATHRILL_JIT_DIVS) || (op == ATHRILL_JIT_REMS);
	int is_div = (op == ATHRILL_JIT_DIVU) || (op == ATHRILL_JIT_DIVS);

	athrill_jit_jump_if(b, ATHRILL_JIT_EQ, c, athrill_jit_imm(0), by_zero);
	if (is_signed) {
		AthrillJitLabel *normal = athrill_jit_label_new(b);

		athrill_jit_jump_if(b, ATHRILL_JIT_NE, c, athrill_jit_imm(-1), normal);
		athrill_jit_jump_if(b, ATHRILL_JIT_EQ, a, athrill_jit_imm(min), overflow);
		athrill_jit_label_place(b, normal);
	}
	athrill_jit_alu(b, op, dst, a, c);
	athrill_jit_jump(b, end);

	athrill_jit_label_place(b, by_zero);
	if (is_div) {
		athrill_jit_mov(b, dst, athrill_jit_imm(-1));
	} else {
		athrill_jit_mov(b, dst, a);
	}
	athrill_jit_jump(b, end);

	athrill_jit_label_place(b, overflow);
	if (is_div) {
		athrill_jit_mov(b, dst, athrill_jit_imm(min));
	} else {
		athrill_jit_mov(b, dst, athrill_jit_imm(0));
	}
	athrill_jit_label_place(b, end);
}

static void test_division(void)
{
	static const AthrillJitAluOp ops[] = {
		ATHRILL_JIT_DIVU, ATHRILL_JIT_DIVS, ATHRILL_JIT_REMU, ATHRILL_JIT_REMS,
	};
	static const char *names[] = { "DIVU", "DIVS", "REMU", "REMS" };
	unsigned k;
	int width;
	int i;
	int j;
	AthrillJitBuilder *b;
	AthrillJitCode *code;
	DivCore core;

	for (k = 0U; k < 4U; k++) {
		for (width = 4; width <= 8; width += 4) {
			b = athrill_jit_builder_create();
			if (width == 4) {
				emit_riscv_div(b, ops[k], width, athrill_jit_core(offsetof(DivCore, q32), 4),
					       athrill_jit_core(offsetof(DivCore, a32), 4), athrill_jit_core(offsetof(DivCore, c32), 4));
			} else {
				emit_riscv_div(b, ops[k], width, athrill_jit_core(offsetof(DivCore, q), 8),
					       athrill_jit_core(offsetof(DivCore, a), 8), athrill_jit_core(offsetof(DivCore, c), 8));
			}
			code = athrill_jit_builder_finish(b);
			CHECK(code != NULL, "div %s/%d: no code (%s)", names[k], width, athrill_jit_last_error());
			if (code == NULL) {
				continue;
			}
			for (i = 0; i < (int)BOUNDARY_NUM + 30; i++) {
				for (j = 0; j < (int)BOUNDARY_NUM + 30; j++) {
					uint64 a = sample(i);
					uint64 c = sample(j);
					uint64 got;
					uint64 want = riscv_div(ops[k], a, c, width);

					core.a = a;
					core.c = c;
					core.a32 = (uint32)a;
					core.c32 = (uint32)c;
					(void)athrill_jit_code_run(code, &core, NULL);
					got = (width == 4) ? core.q32 : core.q;
					CHECK(got == want, "div %s/%d a=%016llx c=%016llx got %016llx want %016llx",
					      names[k], width, (unsigned long long)a, (unsigned long long)c,
					      (unsigned long long)got, (unsigned long long)want);
				}
			}
			athrill_jit_code_free(code);
		}
	}
}

/* ------------------------------------------------------------------ */
/* 5./6. a toy target: interpreter vs JIT                              */

enum {
	TOY_ADDI,	/* x[rd] = x[rs] + imm            (native in the JIT) */
	TOY_ADD,	/* x[rd] = x[rs] + x[rt]          (native) */
	TOY_SUB,	/* x[rd] = x[rs] - x[rt]          (native) */
	TOY_XORSHL,	/* x[rd] = (x[rs] ^ x[rt]) << 1   (call-threaded) */
	TOY_LOAD,	/* x[rd] = mem[x[rs]]; traps outside mem (call-threaded) */
	TOY_BNE,	/* if (x[rs] != x[rt]) pc = imm   (native, ends the block) */
	TOY_HALT,	/* halted = 1                      (call-threaded, ends the block) */
};

typedef struct {
	int op;
	int rd;
	int rs;
	int rt;
	sint32 imm;
} ToyInsn;

#define TOY_MEM_WORDS 16U
#define TOY_BLOCK_MAX 16U

typedef struct {
	uint32 x[8];
	uint32 pc;	/* byte address, 4 per instruction */
	uint32 halted;
	uint32 trap;	/* the number of traps taken */
	uint32 trap_pc;	/* the pc of the last trap */
	uint64 clock;
	uint32 mem[TOY_MEM_WORDS];
} ToyCpu;

static const ToyInsn *toy_prog;
static uint32 toy_prog_len;

/* The interpreter's execute functions; also called from JIT blocks. */
static int toy_exec(void *core_v, void *ctx, const void *arg)
{
	ToyCpu *cpu = (ToyCpu *)core_v;
	const ToyInsn *in = (const ToyInsn *)arg;
	uint32 addr;

	(void)ctx;
	switch (in->op) {
	case TOY_ADDI:
		cpu->x[in->rd] = cpu->x[in->rs] + (uint32)in->imm;
		cpu->pc += 4U;
		return 0;
	case TOY_ADD:
		cpu->x[in->rd] = cpu->x[in->rs] + cpu->x[in->rt];
		cpu->pc += 4U;
		return 0;
	case TOY_SUB:
		cpu->x[in->rd] = cpu->x[in->rs] - cpu->x[in->rt];
		cpu->pc += 4U;
		return 0;
	case TOY_XORSHL:
		cpu->x[in->rd] = (cpu->x[in->rs] ^ cpu->x[in->rt]) << 1;
		cpu->pc += 4U;
		return 0;
	case TOY_LOAD:
		addr = cpu->x[in->rs];
		if (addr >= TOY_MEM_WORDS) {
			/* the guest trap: count it, skip the instruction */
			cpu->trap++;
			cpu->trap_pc = cpu->pc;
			cpu->pc += 4U;
			return 1;
		}
		cpu->x[in->rd] = cpu->mem[addr];
		cpu->pc += 4U;
		return 0;
	case TOY_BNE:
		if (cpu->x[in->rs] != cpu->x[in->rt]) {
			cpu->pc = (uint32)in->imm;
		} else {
			cpu->pc += 4U;
		}
		return 0;
	case TOY_HALT:
		cpu->halted = 1U;
		return 0;
	}
	return 1;
}

/* One instruction: clock first, then the instruction (the target's rule). */
static void toy_step(ToyCpu *cpu)
{
	const ToyInsn *in = &toy_prog[cpu->pc / 4U];

	cpu->clock++;
	(void)toy_exec(cpu, NULL, in);
}

static uint64 toy_run_interp(ToyCpu *cpu, uint64 budget)
{
	uint64 n = 0;

	while ((n < budget) && (cpu->halted == 0U)) {
		toy_step(cpu);
		n++;
	}
	return n;
}

static int toy_ends_block(int op)
{
	return (op == TOY_BNE) || (op == TOY_HALT);
}

#define CPU_FIELD(f) athrill_jit_core(offsetof(ToyCpu, f), (uint8)sizeof(((ToyCpu *)0)->f))
#define CPU_X(n) athrill_jit_core((uint32)(offsetof(ToyCpu, x) + 4U * (uint32)(n)), 4)

static uint32 toy_native_count;
static uint32 toy_call_count;

/*
 * The target's translator. Rules it keeps, as its interpreter does:
 *   - clock and pc are up to date before any call and at every exit
 *     (precise state); natives between them only count;
 *   - a call-threaded instruction whose result is not 0 (a trap), or that
 *     leaves the pc off the next instruction, ends the block.
 */
static AthrillJitCode *toy_translate(uint32 pc)
{
	AthrillJitBuilder *b = athrill_jit_builder_create();
	uint32 n = 0;
	uint32 pending = 0;	/* instructions whose clock is not yet in cpu->clock */
	const ToyInsn *in;
	AthrillJitLabel *taken;

	while (((pc / 4U) < toy_prog_len) && (n < TOY_BLOCK_MAX)) {
		in = &toy_prog[pc / 4U];
		n++;
		athrill_jit_count(b, 1U);
		pending++;
		switch (in->op) {
		case TOY_ADDI:
			athrill_jit_alu(b, ATHRILL_JIT_ADD, CPU_X(in->rd), CPU_X(in->rs), athrill_jit_imm(in->imm));
			toy_native_count++;
			break;
		case TOY_ADD:
			athrill_jit_alu(b, ATHRILL_JIT_ADD, CPU_X(in->rd), CPU_X(in->rs), CPU_X(in->rt));
			toy_native_count++;
			break;
		case TOY_SUB:
			athrill_jit_alu(b, ATHRILL_JIT_SUB, CPU_X(in->rd), CPU_X(in->rs), CPU_X(in->rt));
			toy_native_count++;
			break;
		case TOY_BNE:
			athrill_jit_alu(b, ATHRILL_JIT_ADD, CPU_FIELD(clock), CPU_FIELD(clock), athrill_jit_imm(pending));
			pending = 0;
			taken = athrill_jit_label_new(b);
			athrill_jit_jump_if(b, ATHRILL_JIT_NE, CPU_X(in->rs), CPU_X(in->rt), taken);
			athrill_jit_mov(b, CPU_FIELD(pc), athrill_jit_imm(pc + 4U));
			athrill_jit_exit(b);
			athrill_jit_label_place(b, taken);
			athrill_jit_mov(b, CPU_FIELD(pc), athrill_jit_imm(in->imm));
			athrill_jit_exit(b);
			toy_native_count++;
			break;
		default:
			/* call-threaded: bring the state up to date, call, check */
			athrill_jit_alu(b, ATHRILL_JIT_ADD, CPU_FIELD(clock), CPU_FIELD(clock), athrill_jit_imm(pending));
			pending = 0;
			athrill_jit_mov(b, CPU_FIELD(pc), athrill_jit_imm(pc));
			athrill_jit_call(b, toy_exec, in);
			if (!toy_ends_block(in->op)) {
				athrill_jit_exit_if(b, ATHRILL_JIT_NE, athrill_jit_result(), athrill_jit_imm(0));
				athrill_jit_exit_if(b, ATHRILL_JIT_NE, CPU_FIELD(pc), athrill_jit_imm(pc + 4U));
			}
			toy_call_count++;
			break;
		}
		if (toy_ends_block(in->op)) {
			break;
		}
		pc += 4U;
	}
	/* the block ended on its length: flush clock, set the next pc */
	if (pending != 0U) {
		athrill_jit_alu(b, ATHRILL_JIT_ADD, CPU_FIELD(clock), CPU_FIELD(clock), athrill_jit_imm(pending));
		athrill_jit_mov(b, CPU_FIELD(pc), athrill_jit_imm(pc + 4U));
	}
	return athrill_jit_builder_finish(b);
}

static uint32 toy_count_block(uint32 pc)
{
	uint32 n = 0;

	while (((pc / 4U) < toy_prog_len) && (n < TOY_BLOCK_MAX)) {
		n++;
		if (toy_ends_block(toy_prog[pc / 4U].op)) {
			break;
		}
		pc += 4U;
	}
	return n;
}

/* The dispatch loop a target would have: a block only if it fits the budget. */
static uint64 toy_run_jit(ToyCpu *cpu, uint64 budget, AthrillJitCache *cache, uint64 generation)
{
	uint64 n = 0;
	const AthrillJitCode *code;
	AthrillJitCode *fresh;
	uint32 length;

	while ((n < budget) && (cpu->halted == 0U)) {
		length = toy_count_block(cpu->pc);
		if (length > budget - n) {
			toy_step(cpu);	/* the rest of the budget, one at a time */
			n++;
			continue;
		}
		code = athrill_jit_cache_lookup(cache, cpu->pc, 0U, generation);
		if (code == NULL) {
			fresh = toy_translate(cpu->pc);
			if (fresh == NULL) {
				toy_step(cpu);
				n++;
				continue;
			}
			athrill_jit_cache_insert(cache, cpu->pc, 0U, generation, fresh);
			code = fresh;
		}
		n += athrill_jit_code_run(code, cpu, NULL);
	}
	return n;
}

/* sum = 1 + 2 + ... + x1, with a call-threaded instruction in the loop */
static const ToyInsn toy_sum_prog[] = {
	/* 0x00 */ { TOY_ADDI, 2, 0, 0, 0 },	/* x2 = 0 (counter) */
	/* 0x04 */ { TOY_ADDI, 3, 0, 0, 0 },	/* x3 = 0 (sum) */
	/* 0x08 */ { TOY_ADDI, 2, 2, 0, 1 },	/* loop: x2 += 1 */
	/* 0x0c */ { TOY_ADD, 3, 3, 2, 0 },	/* x3 += x2 */
	/* 0x10 */ { TOY_XORSHL, 4, 3, 2, 0 },	/* x4 = (x3 ^ x2) << 1 (call) */
	/* 0x14 */ { TOY_SUB, 5, 1, 2, 0 },	/* x5 = x1 - x2 */
	/* 0x18 */ { TOY_BNE, 0, 2, 1, 0x08 },	/* if (x2 != x1) goto loop */
	/* 0x1c */ { TOY_HALT, 0, 0, 0, 0 },
};

/* loads through x6 (index), some out of range: traps in the middle of blocks */
static const ToyInsn toy_trap_prog[] = {
	/* 0x00 */ { TOY_ADDI, 6, 0, 0, 0 },	/* x6 = 0 (index) */
	/* 0x04 */ { TOY_ADDI, 7, 0, 0, 40 },	/* x7 = 40 (end) */
	/* 0x08 */ { TOY_ADDI, 6, 6, 0, 3 },	/* loop: x6 += 3 */
	/* 0x0c */ { TOY_LOAD, 5, 6, 0, 0 },	/* x5 = mem[x6] (traps for x6 >= 16) */
	/* 0x10 */ { TOY_ADD, 3, 3, 5, 0 },	/* x3 += x5 */
	/* 0x14 */ { TOY_ADDI, 4, 4, 0, 1 },	/* x4 += 1 (after the trap point) */
	/* 0x18 */ { TOY_BNE, 0, 6, 7, 0x08 },	/* x6 goes 3, 6, ..., 39, 42: never 40 */
	/* 0x1c */ { TOY_HALT, 0, 0, 0, 0 },
};

static void toy_reset(ToyCpu *cpu, uint32 x1)
{
	uint32 i;

	memset(cpu, 0, sizeof(*cpu));
	cpu->x[1] = x1;
	for (i = 0; i < TOY_MEM_WORDS; i++) {
		cpu->mem[i] = 0x1000U + i * 7U;
	}
}

static void toy_compare(const char *what, const ToyCpu *a, const ToyCpu *b, uint64 na, uint64 nb)
{
	CHECK(na == nb, "%s: executed %llu (interp) vs %llu (jit)", what, (unsigned long long)na, (unsigned long long)nb);
	CHECK(memcmp(a, b, sizeof(*a)) == 0,
	      "%s: state differs: pc %x/%x clock %llu/%llu x2 %u/%u x3 %u/%u x4 %u/%u trap %u/%u halted %u/%u",
	      what, a->pc, b->pc, (unsigned long long)a->clock, (unsigned long long)b->clock,
	      a->x[2], b->x[2], a->x[3], b->x[3], a->x[4], b->x[4], a->trap, b->trap, a->halted, b->halted);
}

static void toy_equivalence(const char *name, const ToyInsn *prog, uint32 len, uint32 x1)
{
	/* budgets that end inside blocks, at block ends, and after the end */
	static const uint64 budgets[] = { 1, 2, 3, 5, 7, 8, 13, 100, 1001, 4096, 1000000 };
	unsigned k;
	ToyCpu a;
	ToyCpu b;
	uint64 na;
	uint64 nb;
	char what[96];
	AthrillJitCache *cache = athrill_jit_cache_create(64U);

	toy_prog = prog;
	toy_prog_len = len;
	for (k = 0U; k < sizeof(budgets) / sizeof(budgets[0]); k++) {
		/* one budget for the whole run */
		toy_reset(&a, x1);
		toy_reset(&b, x1);
		na = toy_run_interp(&a, budgets[k]);
		nb = toy_run_jit(&b, budgets[k], cache, 1U);
		(void)snprintf(what, sizeof(what), "%s budget %llu", name, (unsigned long long)budgets[k]);
		toy_compare(what, &a, &b, na, nb);

		/* the same total in slices of that budget (as device events cut it) */
		toy_reset(&a, x1);
		toy_reset(&b, x1);
		na = 0;
		nb = 0;
		while ((a.halted == 0U) && (na < 3000000ULL)) {
			na += toy_run_interp(&a, budgets[k]);
			nb += toy_run_jit(&b, budgets[k], cache, 1U);
		}
		(void)snprintf(what, sizeof(what), "%s slices of %llu", name, (unsigned long long)budgets[k]);
		toy_compare(what, &a, &b, na, nb);
	}
	athrill_jit_cache_destroy(cache);
}

static void test_toy_target(void)
{
	ToyCpu a;
	ToyCpu b;
	uint64 na;
	uint64 nb;
	AthrillJitCache *cache;

	toy_native_count = 0;
	toy_call_count = 0;
	toy_equivalence("sum", toy_sum_prog, sizeof(toy_sum_prog) / sizeof(toy_sum_prog[0]), 1000U);
	toy_equivalence("sum(1)", toy_sum_prog, sizeof(toy_sum_prog) / sizeof(toy_sum_prog[0]), 1U);
	CHECK((toy_native_count > 0U) && (toy_call_count > 0U), "toy: both native and call-threaded instructions were translated");

	/* the sum itself, against arithmetic */
	toy_prog = toy_sum_prog;
	toy_prog_len = sizeof(toy_sum_prog) / sizeof(toy_sum_prog[0]);
	cache = athrill_jit_cache_create(64U);
	toy_reset(&b, 100000U);
	nb = toy_run_jit(&b, 10000000ULL, cache, 1U);
	CHECK(b.x[3] == (uint32)(100000ULL * 100001ULL / 2ULL), "toy sum: %u", b.x[3]);
	CHECK(nb == 2ULL + 5ULL * 100000ULL + 1ULL, "toy sum: %llu instructions", (unsigned long long)nb);
	CHECK(b.clock == nb, "toy sum: clock %llu = instructions", (unsigned long long)b.clock);
	athrill_jit_cache_destroy(cache);

	/* 6. traps in the middle of blocks */
	toy_equivalence("trap", toy_trap_prog, sizeof(toy_trap_prog) / sizeof(toy_trap_prog[0]), 0U);
	toy_prog = toy_trap_prog;
	toy_prog_len = sizeof(toy_trap_prog) / sizeof(toy_trap_prog[0]);
	cache = athrill_jit_cache_create(64U);
	toy_reset(&a, 0U);
	toy_reset(&b, 0U);
	na = toy_run_interp(&a, 25U);	/* x6 = 3, 6, 9, 12, 15, 18: the 6th load traps */
	nb = toy_run_jit(&b, 25U, cache, 1U);
	toy_compare("trap first", &a, &b, na, nb);
	CHECK(b.trap == 0U, "toy trap: no trap within 25 instructions (%u)", b.trap);
	nb += toy_run_jit(&b, 8U, cache, 1U);
	CHECK(b.trap == 1U, "toy trap: one trap (%u)", b.trap);
	CHECK(b.trap_pc == 0x0cU, "toy trap: at the load 0x0c (%x)", b.trap_pc);
	athrill_jit_cache_destroy(cache);
}

/* ------------------------------------------------------------------ */
/* 7. cache                                                            */

static AthrillJitCode *tiny_block(sint64 value)
{
	AthrillJitBuilder *b = athrill_jit_builder_create();
	static uint64 sink;

	athrill_jit_mov(b, athrill_jit_abs(&sink, 8), athrill_jit_imm(value));
	return athrill_jit_builder_finish(b);
}

static void test_cache(void)
{
	AthrillJitCache *cache = athrill_jit_cache_create(20U);	/* rounded up to 32 */
	AthrillJitCacheStats st;
	AthrillJitCode *one = tiny_block(1);
	uint32 i;

	CHECK(athrill_jit_cache_lookup(cache, 0x100U, 0U, 1U) == NULL, "cache: empty");
	athrill_jit_cache_insert(cache, 0x100U, 0U, 1U, one);
	CHECK(athrill_jit_cache_lookup(cache, 0x100U, 0U, 1U) == one, "cache: hit");
	CHECK(athrill_jit_cache_lookup(cache, 0x100U, 1U, 1U) == NULL, "cache: another key misses");
	CHECK(athrill_jit_cache_lookup(cache, 0x104U, 0U, 1U) == NULL, "cache: another pc misses");
	CHECK(athrill_jit_cache_lookup(cache, 0x100U, 0U, 2U) == NULL, "cache: a new generation misses");
	athrill_jit_cache_get_stats(cache, &st);
	CHECK(st.stale == 1U, "cache: one stale (%llu)", (unsigned long long)st.stale);
	CHECK(st.blocks == 0U, "cache: the stale block was freed (%u)", st.blocks);
	CHECK(athrill_jit_cache_lookup(cache, 0x100U, 0U, 1U) == NULL, "cache: a freed block stays gone");

	/* replace in place */
	athrill_jit_cache_insert(cache, 0x100U, 0U, 2U, tiny_block(2));
	athrill_jit_cache_insert(cache, 0x100U, 0U, 3U, tiny_block(3));
	athrill_jit_cache_get_stats(cache, &st);
	CHECK(st.blocks == 1U, "cache: a re-insert replaces (%u)", st.blocks);

	/* fill beyond capacity: one flush, then it keeps working */
	for (i = 0U; i < 40U; i++) {
		athrill_jit_cache_insert(cache, 0x1000U + 4U * i, 0U, 1U, tiny_block((sint64)i));
	}
	athrill_jit_cache_get_stats(cache, &st);
	CHECK(st.flushes == 1U, "cache: one flush when full (%llu)", (unsigned long long)st.flushes);
	CHECK(st.blocks == 40U - 31U, "cache: blocks after the flush (%u)", st.blocks);
	CHECK(athrill_jit_cache_lookup(cache, 0x1000U + 4U * 39U, 0U, 1U) != NULL, "cache: newest block present");
	CHECK(athrill_jit_cache_lookup(cache, 0x1000U, 0U, 1U) == NULL, "cache: flushed block gone");
	CHECK(st.code_bytes > 0U, "cache: code bytes counted");

	athrill_jit_cache_flush(cache);
	athrill_jit_cache_get_stats(cache, &st);
	CHECK((st.blocks == 0U) && (st.code_bytes == 0U), "cache: flush empties (%u, %llu)", st.blocks, (unsigned long long)st.code_bytes);
	athrill_jit_cache_destroy(cache);
}

/* ------------------------------------------------------------------ */
/* 8. invalid descriptions                                             */

static void test_invalid(void)
{
	AthrillJitBuilder *b;
	AthrillJitLabel *never;
	uint64 v = 0;

	b = athrill_jit_builder_create();
	athrill_jit_alu(b, ATHRILL_JIT_DIVU, athrill_jit_abs(&v, 8), athrill_jit_abs(&v, 8), athrill_jit_imm(0));
	CHECK(athrill_jit_builder_finish(b) == NULL, "invalid: division by immediate 0 refused");
	CHECK(strstr(athrill_jit_last_error(), "division") != NULL, "invalid: message (%s)", athrill_jit_last_error());

	b = athrill_jit_builder_create();
	athrill_jit_alu(b, ATHRILL_JIT_DIVS, athrill_jit_abs(&v, 8), athrill_jit_abs(&v, 8), athrill_jit_imm(-1));
	CHECK(athrill_jit_builder_finish(b) == NULL, "invalid: signed division by immediate -1 refused");

	b = athrill_jit_builder_create();
	never = athrill_jit_label_new(b);
	athrill_jit_jump(b, never);
	CHECK(athrill_jit_builder_finish(b) == NULL, "invalid: label never placed refused");

	b = athrill_jit_builder_create();
	athrill_jit_mov(b, athrill_jit_imm(1), athrill_jit_imm(2));
	CHECK(athrill_jit_builder_finish(b) == NULL, "invalid: immediate destination refused");

	b = athrill_jit_builder_create();
	athrill_jit_mov(b, athrill_jit_abs(&v, 3), athrill_jit_imm(2));
	CHECK(athrill_jit_builder_finish(b) == NULL, "invalid: width 3 refused");
}

int main(void)
{
#ifndef _WIN32
	/* a broken JIT can loop forever: end the test instead of hanging */
	(void)alarm(60U);
#endif
	printf("athrill_jit unit tests\n");
	test_alu();
	printf("  alu: %d checks so far\n", checks);
	test_mov();
	test_conditions();
	test_exit_if();
	test_call();
	printf("  conditions, exit_if: %d checks so far\n", checks);
	test_division();
	printf("  division: %d checks so far\n", checks);
	test_toy_target();
	printf("  toy target: %d checks so far\n", checks);
	test_cache();
	test_invalid();
	printf("%s: %d checks, %d failures\n", failures == 0 ? "PASS" : "FAIL", checks, failures);
	return failures == 0 ? 0 : 1;
}
