/*
 * The indexed symbol lookups find a symbol whose range [addr, addr + size)
 * contains the address, and -1 exactly when no symbol does - checked
 * against a linear search with overlapping, nested, zero-sized and
 * duplicate symbols, and with symbols added between lookups. (With
 * overlapping symbols the last-hit cache may return another containing
 * symbol than the lowest id, as before.)
 */
#include "symbol_ops.h"
#include <stdio.h>
#include <stdlib.h>

#define SYMBOLS 3000U
#define LOOKUPS 200000U

static DbgSymbolType reference[SYMBOLS];
static unsigned count;

static int linear_find(AthrillAddrType addr)
{
	unsigned id;

	for (id = 0; id < count; id++) {
		if ((addr >= reference[id].addr) && (addr < reference[id].addr + reference[id].size)) {
			return (int)id;
		}
	}
	return -1;
}

static int matches(int found, int expected, AthrillAddrType addr)
{
	if (expected < 0) {
		return found == -1;
	}
	return (found >= 0) && (addr >= reference[found].addr)
		&& (addr < reference[found].addr + reference[found].size);
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

int main(void)
{
	unsigned i;
	unsigned failures = 0;
	AthrillAddrType addr;
	AthrillAddrType found_addr;
	int expected;
	int found;

	srand(1);
	for (i = 0; i < SYMBOLS; i++) {
		/* Mostly small, some large and nested, some zero-sized or duplicated. */
		uint32 size = (i % 97U == 0U) ? 0x4000U : (uint32)(rand() % 64);
		AthrillAddrType start = (i % 50U == 0U && count > 0U) ? reference[count - 1U].addr
			: (AthrillAddrType)(rand() % 0x20000);
		add_symbol(start, size);
		if (i % 500U == 0U) {
			addr = (AthrillAddrType)(rand() % 0x21000);
			expected = linear_find(addr);
			if (!matches(symbol_addr2glid(addr, &found_addr), expected, addr)
				|| !matches(symbol_pc2funcid(addr, &found_addr), expected, addr)) {
				failures++;
			}
		}
	}
	for (i = 0; i < LOOKUPS; i++) {
		addr = (AthrillAddrType)(rand() % 0x21000);
		expected = linear_find(addr);
		found = symbol_addr2glid(addr, &found_addr);
		if (!matches(found, expected, addr) || ((found >= 0) && (found_addr != reference[found].addr))) {
			failures++;
		}
		found = symbol_pc2funcid(addr, &found_addr);
		if (!matches(found, expected, addr) || ((found >= 0) && (found_addr != reference[found].addr))) {
			failures++;
		}
	}
	printf("%s (%u mismatches)\n", failures == 0U ? "SELFTEST PASS" : "SELFTEST FAIL", failures);
	return failures == 0U ? 0 : 1;
}
