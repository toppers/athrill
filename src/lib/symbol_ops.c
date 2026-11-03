#include "cpuemu_config.h"
#include "symbol_ops.h"
#include "std_types.h"
#include "assert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint32 symbolc_func_max_num = 0;
static uint32 symbolc_gl_max_num = 0;

static uint32 symbol_func_size = 0;
static uint32 symbol_gl_size = 0;
static DbgSymbolType *symbol_func;
static DbgSymbolType *symbol_gl;

uint32 symbol_get_func_num(void)
{
	return symbol_func_size;
}
uint32 symbol_get_gl_num(void)
{
	return symbol_gl_size;
}
int symbol_gl_add(DbgSymbolType *sym)
{
	if (symbol_gl_size >= symbolc_gl_max_num) {
		symbolc_gl_max_num += CPUEMU_CONFIG_OBJECT_SYMBOL_TABLE_NUM;
		symbol_gl = realloc(symbol_gl, symbolc_gl_max_num * sizeof(DbgSymbolType));
		ASSERT(symbol_gl != NULL);
	}
	symbol_gl[symbol_gl_size] = *sym;
	symbol_gl_size++;
	return 0;
}

int symbol_func_add(DbgSymbolType *sym)
{
	if (symbol_func_size >= symbolc_func_max_num) {
		symbolc_func_max_num += CPUEMU_CONFIG_OBJECT_SYMBOL_TABLE_NUM;
		symbol_func = realloc(symbol_func, symbolc_func_max_num * sizeof(DbgSymbolType));
		ASSERT(symbol_func != NULL);
	}
	symbol_func[symbol_func_size] = *sym;
	symbol_func_size++;
	return 0;
}


/*
 * Address lookup for the per-clock debugger features (backtrace, function
 * trace, profiling). The tables are searched with an index of symbol ids
 * sorted by start address, rebuilt when symbols were added.
 *
 * Result of symbol_addr2glid() and symbol_pc2funcid() (each keeps its own
 * last result): the symbol returned last if its range [addr, addr + size)
 * still contains the address, otherwise the lowest id whose range contains
 * it, or -1. (Before the index, a last result of id 0 was not reused.)
 * max_end[k] is the largest end among order[0..k], which bounds the
 * backward scan over symbols that may still contain the address.
 */
typedef struct {
	uint32 built_count;
	uint32 *order;
	AthrillAddrType *max_end;
} SymbolRangeIndex;

static SymbolRangeIndex symbol_func_index;
static SymbolRangeIndex symbol_gl_index;
static const DbgSymbolType *symbol_sort_table;

static int symbol_compare_start(const void *left, const void *right)
{
	uint32 a = *(const uint32 *)left;
	uint32 b = *(const uint32 *)right;

	if (symbol_sort_table[a].addr != symbol_sort_table[b].addr) {
		return symbol_sort_table[a].addr < symbol_sort_table[b].addr ? -1 : 1;
	}
	return a < b ? -1 : (a > b ? 1 : 0);
}

static void symbol_index_build(SymbolRangeIndex *index, const DbgSymbolType *table, uint32 count)
{
	AthrillAddrType max_end = 0;
	AthrillAddrType end;
	uint32 k;

	index->order = realloc(index->order, (count + 1U) * sizeof(uint32));
	index->max_end = realloc(index->max_end, (count + 1U) * sizeof(AthrillAddrType));
	ASSERT((index->order != NULL) && (index->max_end != NULL));
	for (k = 0; k < count; k++) {
		index->order[k] = k;
	}
	symbol_sort_table = table;
	qsort(index->order, count, sizeof(uint32), symbol_compare_start);
	for (k = 0; k < count; k++) {
		end = table[index->order[k]].addr + table[index->order[k]].size;
		if (end > max_end) {
			max_end = end;
		}
		index->max_end[k] = max_end;
	}
	index->built_count = count;
}

static int symbol_index_find(SymbolRangeIndex *index, const DbgSymbolType *table, uint32 count,
	AthrillAddrType addr)
{
	uint32 low = 0;
	uint32 high = count;
	uint32 middle;
	uint32 k;
	int found = -1;
	const DbgSymbolType *symbol;

	if (index->built_count != count) {
		symbol_index_build(index, table, count);
	}
	/* low = number of symbols whose start address is <= addr */
	while (low < high) {
		middle = low + (high - low) / 2U;
		if (table[index->order[middle]].addr <= addr) {
			low = middle + 1U;
		}
		else {
			high = middle;
		}
	}
	for (k = low; k > 0U; k--) {
		if (index->max_end[k - 1U] <= addr) {
			break;
		}
		symbol = &table[index->order[k - 1U]];
		if ((addr < symbol->addr + symbol->size)
			&& ((found < 0) || (index->order[k - 1U] < (uint32)found))) {
			found = (int)index->order[k - 1U];
		}
	}
	return found;
}

int symbol_get_func(char *funcname, uint32 func_len, AthrillAddrType *addrp, uint32 *size)
{
	int i;
	uint32 len;

	for (i = 0; i < symbol_func_size; i++) {
		len = strlen(symbol_func[i].name);

		if (func_len != len) {
			continue;
		}
		if (strncmp(funcname, symbol_func[i].name, func_len) != 0) {
			continue;
		}
		*addrp = symbol_func[i].addr;
		*size = symbol_func[i].size;
		return 0;
	}
	return -1;
}
char * symbol_pc2func(AthrillAddrType pc)
{
	int i;

	for (i = 0; i < symbol_func_size; i++) {
		if (pc < symbol_func[i].addr) {
			continue;
		}
		if (pc >= (symbol_func[i].addr + symbol_func[i].size)) {
			continue;
		}
		return  symbol_func[i].name;
	}
	return NULL;
}
void symbol_set_pc(int funcid, uint32 core_id, uint32 sp)
{
	symbol_func[funcid].enter_sp[core_id] = sp;
	return;
}
uint32 symbol_get_entered_sp(int funcid, uint32 core_id)
{
	return symbol_func[funcid].enter_sp[core_id];
}

int symbol_pc2funcid(AthrillAddrType pc, AthrillAddrType *funcaddr)
{
	int found;
	static int last_funcid = -1;

	if (last_funcid >= 0) {
		if ((pc >= symbol_func[last_funcid].addr) &&
				(pc < (symbol_func[last_funcid].addr + symbol_func[last_funcid].size))) {
			*funcaddr = symbol_func[last_funcid].addr;
			return last_funcid;
		}
	}
	found = symbol_index_find(&symbol_func_index, symbol_func, symbol_func_size, pc);
	if (found >= 0) {
		*funcaddr = symbol_func[found].addr;
		last_funcid = found;
	}
	return found;
}
char * symbol_funcid2funcname(int id)
{
	if (id < 0) {
		return "unknown";
	}
	return symbol_func[id].name;
}
AthrillAddrType symbol_funcid2funcaddr(int id)
{
	return symbol_func[id].addr;
}
uint32 symbol_funcid2funcsize(int id)
{
	return symbol_func[id].size;
}


int symbol_get_gl(char *gl_name, uint32 gl_len, AthrillAddrType *addrp, uint32 *size)
{
	int i;
	uint32 len;

	for (i = 0; i < symbol_gl_size; i++) {
		len = strlen(symbol_gl[i].name);

		if (gl_len != len) {
			continue;
		}
		if (strncmp(gl_name, symbol_gl[i].name, gl_len) != 0) {
			continue;
		}
		*addrp = symbol_gl[i].addr;
		*size = symbol_gl[i].size;
		return i;
	}
	return -1;
}

int symbol_addr2glid(AthrillAddrType addr, AthrillAddrType *gladdr)
{
	int found;
	static int last_glid = -1;

	if (last_glid >= 0) {
		if ((addr >= symbol_gl[last_glid].addr) &&
				(addr < (symbol_gl[last_glid].addr + symbol_gl[last_glid].size))) {
			*gladdr = symbol_gl[last_glid].addr;
			return last_glid;
		}
	}
	found = symbol_index_find(&symbol_gl_index, symbol_gl, symbol_gl_size, addr);
	if (found >= 0) {
		*gladdr = symbol_gl[found].addr;
		last_glid = found;
	}
	return found;
}

char * symbol_glid2glname(int id)
{
	return symbol_gl[id].name;
}

AthrillAddrType symbol_glid2gladdr(int id)
{
	return symbol_gl[id].addr;
}

void symbol_print_gl(char *gl_name, uint32 show_num)
{
	int i;
	uint32 len;
	uint32 gl_len;

	gl_len = strlen(gl_name);
	for (i = 0; i < symbol_gl_size; i++) {
		len = strlen(symbol_gl[i].name);
		if (len < gl_len) {
			continue;
		}

		if (strncmp(gl_name, symbol_gl[i].name, gl_len) != 0) {
			continue;
		}
		if (show_num > 0) {
			printf("candidate %s\n", symbol_gl[i].name);
			show_num--;
		}
		else {
			break;
		}
	}
	return;
}


void symbol_print_func(char *gl_name, uint32 show_num)
{
	int i;
	uint32 len;
	uint32 gl_len;

	gl_len = strlen(gl_name);
	for (i = 0; i < symbol_func_size; i++) {
		len = strlen(symbol_func[i].name);
		if (len < gl_len) {
			continue;
		}

		if (strncmp(gl_name, symbol_func[i].name, gl_len) != 0) {
			continue;
		}
		if (show_num > 0) {
			printf("candidate %s\n", symbol_func[i].name);
			show_num--;
		}
		else {
			break;
		}
	}
	return;
}
