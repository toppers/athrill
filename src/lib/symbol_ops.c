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


int symbol_get_func(char *funcname, uint32 func_len, uint32 *addrp, uint32 *size)
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
char * symbol_pc2func(uint32 pc)
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

int symbol_pc2funcid(uint32 pc, uint32 *funcaddr)
{
	int i;
	static int last_funcid = -1;

	if (last_funcid > 0) {
		if ((pc >= symbol_func[last_funcid].addr) &&
				(pc < (symbol_func[last_funcid].addr + symbol_func[last_funcid].size))) {
			*funcaddr = symbol_func[last_funcid].addr;
			return last_funcid;
		}
	}

	for (i = 0; i < symbol_func_size; i++) {
		if (pc < symbol_func[i].addr) {
			continue;
		}
		if (pc >= (symbol_func[i].addr + symbol_func[i].size)) {
			continue;
		}
		*funcaddr = symbol_func[i].addr;
		last_funcid = i;
		return  i;
	}
	return -1;
}
char * symbol_funcid2funcname(int id)
{
	if (id < 0) {
		return "unknown";
	}
	return symbol_func[id].name;
}
uint32 symbol_funcid2funcaddr(int id)
{
	return symbol_func[id].addr;
}
uint32 symbol_funcid2funcsize(int id)
{
	return symbol_func[id].size;
}


int symbol_get_gl(char *gl_name, uint32 gl_len, uint32 *addrp, uint32 *size)
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

int symbol_addr2glid(uint32 addr, uint32 *gladdr)
{
	int i;
	static int last_funcid = -1;

	if (last_funcid > 0) {
		if ((addr >= symbol_gl[last_funcid].addr) &&
				(addr < (symbol_gl[last_funcid].addr + symbol_gl[last_funcid].size))) {
			*gladdr = symbol_gl[last_funcid].addr;
			return last_funcid;
		}
	}

	for (i = 0; i < symbol_gl_size; i++) {
		if (addr < symbol_gl[i].addr) {
			continue;
		}
		if (addr >= (symbol_gl[i].addr + symbol_gl[i].size)) {
			continue;
		}
		*gladdr = symbol_gl[i].addr;
		last_funcid = i;
		return  i;
	}
	return -1;

}

char * symbol_glid2glname(int id)
{
	return symbol_gl[id].name;
}

uint32 symbol_glid2gladdr(int id)
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
