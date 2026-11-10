/*
 * symbol_addr2glid() and symbol_pc2funcid() against reference models, with
 * overlapping, nested, zero-sized and duplicate symbols and with symbols
 * added between lookups.
 *
 * Specification (symbol_ops.c): the symbol returned last if it still
 * contains the address, otherwise the lowest id containing it, or -1.
 * The results must equal that model exactly. They must also equal the
 * previous linear implementation, except where it did not reuse a last
 * result of id 0.
 *
 * Also symbol_registered_name(): the leading underscore dropped unless
 * kept, a name without one (or empty) kept whole.
 */
#include "symbol_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYMBOLS 3000U
#define LOOKUPS 200000U

static DbgSymbolType reference[SYMBOLS];
static unsigned count;

static int contains(int id, AthrillAddrType addr)
{
	return (id >= 0) && (addr >= reference[id].addr)
		&& (addr < reference[id].addr + reference[id].size);
}

static int lowest_containing(AthrillAddrType addr)
{
	unsigned id;

	for (id = 0; id < count; id++) {
		if (contains((int)id, addr)) {
			return (int)id;
		}
	}
	return -1;
}

/* The specification: reuse the last result while it contains the address. */
static int specified_lookup(int *last, AthrillAddrType addr)
{
	int found;

	if ((*last >= 0) && contains(*last, addr)) {
		return *last;
	}
	found = lowest_containing(addr);
	if (found >= 0) {
		*last = found;
	}
	return found;
}

/* The previous implementation: the same, but a last result of 0 was not reused. */
static int previous_lookup(int *last, AthrillAddrType addr)
{
	int found;

	if ((*last > 0) && contains(*last, addr)) {
		return *last;
	}
	found = lowest_containing(addr);
	if (found >= 0) {
		*last = found;
	}
	return found;
}

static void add_symbol(AthrillAddrType addr, uint32 size)
{
	static char name[] = "s";
	DbgSymbolType symbol = { name, addr, size, { 0 } };

	reference[count] = symbol;
	count++;
	(void)symbol_gl_add(&symbol);
	(void)symbol_func_add(&symbol);
}

static unsigned spec_mismatches;
static unsigned previous_mismatches;
static unsigned previous_differences_not_id0;
static int spec_gl_last = -1;
static int spec_func_last = -1;
static int previous_gl_last = -1;
static int previous_func_last = -1;

static void compare(int actual, int specified, int previous, int previous_last_before,
	AthrillAddrType found_addr)
{
	if ((actual != specified) || ((actual >= 0) && (found_addr != reference[actual].addr))) {
		spec_mismatches++;
	}
	if (actual != previous) {
		previous_mismatches++;
		if (previous_last_before != 0) {
			previous_differences_not_id0++;
		}
	}
}

static void lookup(AthrillAddrType addr)
{
	AthrillAddrType found_addr = 0;
	int previous_before;
	int actual;
	int specified;
	int previous;

	previous_before = previous_gl_last;
	actual = symbol_addr2glid(addr, &found_addr);
	specified = specified_lookup(&spec_gl_last, addr);
	previous = previous_lookup(&previous_gl_last, addr);
	compare(actual, specified, previous, previous_before, found_addr);

	previous_before = previous_func_last;
	actual = symbol_pc2funcid(addr, &found_addr);
	specified = specified_lookup(&spec_func_last, addr);
	previous = previous_lookup(&previous_func_last, addr);
	compare(actual, specified, previous, previous_before, found_addr);
}

static unsigned name_failures;

static void check_name(const char *elf_name, std_bool keep_prefix, const char *want)
{
	const char *got = symbol_registered_name(elf_name, keep_prefix);

	if (strcmp(got, want) != 0) {
		printf("registered name of \"%s\" (keep %d): \"%s\", want \"%s\"\n", elf_name, (int)keep_prefix, got,
		       want);
		name_failures++;
	}
}

int main(void)
{
	unsigned i;

	check_name("_main", FALSE, "main");
	check_name("__start", FALSE, "_start");
	check_name("arch_cpu_idle", FALSE, "arch_cpu_idle");
	check_name("$t", FALSE, "$t");
	check_name("", FALSE, "");
	check_name("_main", TRUE, "_main");
	check_name("arch_cpu_idle", TRUE, "arch_cpu_idle");
	printf("registered-name mismatches: %u\n", name_failures);

	srand(1);
	for (i = 0; i < SYMBOLS; i++) {
		/* Mostly small, some large and nested, some zero-sized or duplicated. */
		uint32 size = (i % 97U == 0U) ? 0x4000U : (uint32)(rand() % 64);
		AthrillAddrType start = ((i % 50U == 0U) && (count > 0U)) ? reference[count - 1U].addr
			: (AthrillAddrType)(rand() % 0x20000);
		add_symbol(start, size);
		if (i % 500U == 0U) {
			lookup((AthrillAddrType)(rand() % 0x21000));
		}
	}
	for (i = 0; i < LOOKUPS; i++) {
		/* Runs of nearby addresses, like a stack pointer, then jumps. */
		AthrillAddrType base = (AthrillAddrType)(rand() % 0x21000);
		unsigned step;
		for (step = 0; step < 4U; step++) {
			lookup(base + (AthrillAddrType)(rand() % 16));
		}
	}
	printf("specification mismatches: %u\n", spec_mismatches);
	printf("differences from the previous implementation: %u (other than its id 0 case: %u)\n",
		previous_mismatches, previous_differences_not_id0);
	if ((spec_mismatches == 0U) && (previous_differences_not_id0 == 0U) && (name_failures == 0U)) {
		printf("SELFTEST PASS\n");
		return 0;
	}
	printf("SELFTEST FAIL\n");
	return 1;
}
