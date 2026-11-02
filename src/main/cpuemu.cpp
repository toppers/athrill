#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "cpu.h"
#include "bus.h"
#include "cpuemu_ops.h"
#include "hakoniwa/athrill_hakoniwa.h"
#include "bus.h"
#include "std_device_ops.h"
#include "std_cpu_ops.h"
#include "dbg_log.h"
#include "cpu_control/dbg_cpu_control.h"
#include "cpu_control/dbg_cpu_thread_control.h"
#include "cpu_control/dbg_cpu_callback.h"
#include "elf_section.h"
#include "symbol_ops.h"
#include "token.h"
#include "file.h"
#include "osdep.h"
#include "target/target_os_api.h"
#include "option/option.h"
#include "shared_library.h"
#include "athrill_device.h"
#include "assert.h"
#include "athrill_exdev_header.h"
}

static DeviceClockType cpuemu_dev_clock;
std_bool private_cpuemu_is_cui_mode = FALSE;
static uint64 cpuemu_cpu_end_clock = -1LLU;
static AthrillAddrType cpuemu_cpu_entry_addr = 0U;
static void cpuemu_env_parse_devcfg_string(TokenStringType* strp);

Std_ReturnType cpuemu_symbol_set(void)
{
	uint32 i;
	uint32 num;
	Std_ReturnType err;
	DbgSymbolType sym;
	ElfSymbolType elfsym;
	std_bool disable_underscore = FALSE;
	uint32 name_index = 1U;

	(void)cpuemu_get_devcfg_value("DISABLE_SYMBOL_UNDERSCORE", (uint32*)&disable_underscore);
	if (disable_underscore != FALSE) {
		name_index = 0U;
	}


	err = elfsym_get_symbol_num(&num);
	if (err != STD_E_OK) {
		return err;
	}

	for (i = 0; i < num; i++) {
		err = elfsym_get_symbol(i, &elfsym);
		if (err != STD_E_OK) {
			return err;
		}
		sym.name = &elfsym.name[name_index];
		sym.addr = elfsym.addr;
		sym.size = elfsym.size;
		switch (elfsym.type) {
		case SYMBOL_TYPE_OBJECT:
			if (symbol_gl_add(&sym) < 0) {
				return STD_E_INVALID;
			}
			break;
		case SYMBOL_TYPE_FUNC:
			if (symbol_func_add(&sym) < 0) {
				return STD_E_INVALID;
			}
			break;
		case SYMBOL_TYPE_NOTYPE:
			if (sym.name[0] != '$') {
				if (symbol_gl_add(&sym) < 0) {
					return STD_E_INVALID;
				}
				if (symbol_func_add(&sym) < 0) {
					return STD_E_INVALID;
				}
			}
			break;
		default:
			//printf("undefined symbol:%s type:0x%x\n", &elfsym.name[name_index], elfsym.type);
			break;
		}

	}

	return STD_E_OK;
}

int cpu_config_get_core_id_num(void)
{
	return (int)virtual_cpu.core_id_num;
}

void cpuemu_init(void *(*cpu_run)(void *), void *opt)
{
	CmdOptionType *copt = (CmdOptionType*)opt;
	CoreIdType i;
	dbg_log_init((char*)"./log.txt");
	if (copt->core_id_num > 0) {
		virtual_cpu.core_id_num = copt->core_id_num;
	}
	else {
		virtual_cpu.core_id_num = CPU_CONFIG_CORE_NUM;
	}
	osdep_elaps_reset(&cpuemu_dev_clock);

	cpu_init();
	device_init(&virtual_cpu, &cpuemu_dev_clock);
	cputhr_control_init();
	cpuctrl_init();
	if (cpu_run != NULL) {
		private_cpuemu_is_cui_mode = TRUE;
		for (i = 0; i < cpu_config_get_core_id_num(); i++) {
			dbg_cpu_debug_mode_set(i, TRUE);
		}
		cputhr_control_start(cpu_run);
	}
	else {
		private_cpuemu_is_cui_mode = FALSE;
	}
#ifdef EXDEV_ENABLE
	device_init_athrill_device();
#endif /* EXDEV_ENABLE */
	return;
}

uint64 cpuemu_get_cpu_end_clock(void)
{
	return cpuemu_cpu_end_clock;
}
void cpuemu_set_cpu_end_clock(uint64 clock)
{
	cpuemu_cpu_end_clock = clock;
	return;
}

void cpuemu_set_entry_addr(AthrillAddrType entry_addr)
{
	cpuemu_cpu_entry_addr = entry_addr;
	return;
}

void cpuemu_reset(void)
{
	osdep_elaps_reset(&cpuemu_dev_clock);
	cpu_init();
	device_reset(&virtual_cpu, &cpuemu_dev_clock);
	for (CoreIdType i = 0; i < virtual_cpu.core_id_num; i++) {
		cpu_set_core_pc(i, cpuemu_cpu_entry_addr);
	}
}

void cpuemu_get_elaps(CpuEmuElapsType *elaps)
{
	int core_id;
	elaps->core_id_num = cpu_config_get_core_id_num();
	elaps->total_clocks = cpuemu_dev_clock.clock;
	elaps->intr_clocks = cpuemu_dev_clock.intclock;
	osdep_elaps_copy(&cpuemu_dev_clock, elaps);
	for (core_id = 0; core_id < elaps->core_id_num; core_id++) {
		elaps->cpu_clocks[core_id] = virtual_cpu.cores[core_id].elaps;
	}
	return;
}

void cpuemu_start_elaps(void)
{
	osdep_elaps_start(&cpuemu_dev_clock);
	return;
}

void cpuemu_end_elaps(void)
{
	osdep_elaps_end(&cpuemu_dev_clock);
	return;
}

uint32 cpuemu_get_retaddr(CoreIdType core_id)
{
	return cpu_get_return_addr((const TargetCoreType *)&virtual_cpu.cores[core_id]);
}

Std_ReturnType cpuemu_get_addr_pointer(AthrillAddrType addr, uint8 **data)
{
	return bus_get_pointer(CPU_CONFIG_CORE_ID_0, addr, data);
}


void cpuemu_get_register(CoreIdType core_id, TargetCoreType *cpu)
{
	*cpu = virtual_cpu.cores[core_id].core;
	return;
}

static TokenContainerType cpuemu_rom_parameter;

static void cpuemu_set_debug_romdata(void)
{
	uint32 param_num;
	uint32 i;
	Std_ReturnType err;
	char *rom_variable_name;
	static char parameter[4096];

	err = cpuemu_get_devcfg_value("DEBUG_ROM_DEFINE_NUM", &param_num);
	if (err != STD_E_OK) {
		return;
	}
	std::cout << "rom_param_num=" << param_num << '\n';
	for (i = 0; i < param_num; i++) {
		snprintf(parameter, sizeof(parameter), "DEBUG_ROM_DEFINE_%d", i);
		err = cpuemu_get_devcfg_string(parameter, &rom_variable_name);
		if (err != STD_E_OK) {
			std::cout << "not found param=" << parameter << '\n';
			return;
		}
		err = token_split_with_delimiter(&cpuemu_rom_parameter, (uint8*)rom_variable_name, strlen(rom_variable_name), '=');
		if (err != STD_E_OK) {
			std::cout << "can not get param value=" << rom_variable_name << '\n';
			return;
		}
		std::cout << "ROM:redefine " << cpuemu_rom_parameter.array[0].body.str.str
			<< " = " << cpuemu_rom_parameter.array[1].body.dec.value << '\n';
		{
			int ret;
			char *gl_name = (char*)cpuemu_rom_parameter.array[0].body.str.str;
			uint32 gl_len = strlen(gl_name);
			AthrillAddrType addr;
			uint32 size;
			uint8* datap;
			ret = symbol_get_gl(gl_name, gl_len, &addr, &size);
			if (ret < 0) {
				std::cout << "can not found global variable=" << gl_name << '\n';
				return;
			}
			err = bus_get_pointer(0U, addr, &datap);
			if (err != 0) {
				std::cout << "can not found memory pointer=" << gl_name << '\n';
				return;
			}
			switch (size) {
			case 1:
				*((uint8*)datap) = (uint8)cpuemu_rom_parameter.array[1].body.dec.value;
				break;
			case 2:
				*((uint16*)datap) = (uint16)cpuemu_rom_parameter.array[1].body.dec.value;
				break;
			case 4:
				*((uint32*)datap) = (uint32)cpuemu_rom_parameter.array[1].body.dec.value;
				break;
			default:
				std::cout << "can not set pointer because gl_size(" << gl_name << ':' << size << ") > 4\n";
				break;
			}
		}
	}
	return;
}


static DbgCpuCallbackFuncEnableType enable_dbg;

static inline std_bool cpuemu_thread_run_nodbg(int core_id_num)
{
	std_bool is_halt;
	CoreIdType i = 0;
	Std_ReturnType err;
	/**
	 * デバイス実行実行
	 */
#ifdef EXDEV_ENABLE
	device_supply_clock_athrill_device();
#endif /* EXDEV_ENABLE */
	device_supply_clock(&cpuemu_dev_clock);
	if (athrill_hakoniwa_stage(cpuemu_dev_clock.clock, virtual_cpu.cpu_freq,
		&cpuemu_dev_clock.min_intr_interval) == FALSE) {
		return TRUE;
	}

	/**
	 * CPU 実行
	 */
	is_halt = TRUE;
	/* TODO: The single-CPU optimisation was never active because its guard was misspelled. */
	for (i = 0; i < core_id_num; i++) {
		virtual_cpu.current_core = &virtual_cpu.cores[i];

		err = cpu_supply_clock(i);
		if ((err != STD_E_OK) && (cpu_illegal_access(i) == FALSE)) {
			std::cout << "CPU(pc=0x" << std::hex << cpu_get_pc(&virtual_cpu.cores[i].core) << std::dec << ") Exception!!\n";
			fflush(stdout);
			exit(1);
		}
		/**
		 * CPU 実行完了通知
		 */
		if (virtual_cpu.cores[i].core.is_halt != TRUE) {
			is_halt = FALSE;
		}
	}
	cpuemu_dev_clock.is_halt = is_halt;
	return is_halt;
}
static inline std_bool cpuemu_thread_run_dbg(int core_id_num)
{
	std_bool is_halt;
	CoreIdType i;
	Std_ReturnType err;


	/**
	 * デバイス実行実行
	 */
#ifdef EXDEV_ENABLE
	device_supply_clock_athrill_device();
#endif /* EXDEV_ENABLE */
	device_supply_clock(&cpuemu_dev_clock);
	if (athrill_hakoniwa_stage(cpuemu_dev_clock.clock, virtual_cpu.cpu_freq,
		&cpuemu_dev_clock.min_intr_interval) == FALSE) {
		return TRUE;
	}

	/**
	 * CPU 実行
	 */
	is_halt = TRUE;
	for (i = 0; i < core_id_num; i++) {
		virtual_cpu.current_core = &virtual_cpu.cores[i];

		/*
		 * バスのアクセスログをクリアする
		 */
		bus_access_set_log(BUS_ACCESS_TYPE_NONE, 8U, 0, 0);

		/**
		 * CPU 実行開始通知
		 */
		dbg_cpu_callback_start(cpu_get_pc(&virtual_cpu.cores[i].core), cpu_get_sp(&virtual_cpu.cores[i].core));

		dbg_notify_cpu_clock_supply_start(&virtual_cpu.cores[i].core);

		err = cpu_supply_clock(i);
		if ((err != STD_E_OK) && (cpu_illegal_access(i) == FALSE)) {
			std::cout << "CPU(pc=0x" << std::hex << cpu_get_pc(&virtual_cpu.cores[i].core) << std::dec << ") Exception!!\n";
			fflush(stdout);
			cpuctrl_set_force_break();
		}
		/**
		 * CPU 実行完了通知
		 */
		dbg_notify_cpu_clock_supply_end(&virtual_cpu.cores[i].core, &enable_dbg);

		if (virtual_cpu.cores[i].core.is_halt != TRUE) {
			is_halt = FALSE;
		}
	}
	cpuemu_dev_clock.is_halt = is_halt;
	return is_halt;
}


/*
 * Optionally keep pace with wall-clock time and report the skipped clocks
 * before the main loop jumps to the next device interrupt.
 */
static void cpuemu_sync_skipped_clock(void)
{
	osdep_sync_skipped_clock(cpuemu_dev_clock.min_intr_interval,
		virtual_cpu.cpu_freq, enable_dbg.enable_sync_time, enable_dbg.show_skip_time);
	return;
}

void *cpuemu_thread_run(void* arg)
{
	std_bool is_halt;
	int core_id_num = cpu_config_get_core_id_num();
	static std_bool (*do_cpu_run) (int);
	int core_id;

	enable_dbg.enable_bt = TRUE;
	enable_dbg.enable_ft = TRUE;
	enable_dbg.enable_watch = TRUE;
	enable_dbg.enable_prof = TRUE;
	enable_dbg.enable_sync_time = FALSE;
	enable_dbg.reset_pc = cpuemu_cpu_entry_addr;
	cpuemu_dev_clock.enable_skip = FALSE;

	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_ENABLE_BT", &enable_dbg.enable_bt);
	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_ENABLE_FT", &enable_dbg.enable_ft);
	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_ENABLE_PROF", &enable_dbg.enable_watch);
	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_ENABLE_WATCH", &enable_dbg.enable_prof);
	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_ENABLE_SYNC_TIME", &enable_dbg.enable_sync_time);
	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_SHOW_SKIP_TIME", &enable_dbg.show_skip_time);
	(void)cpuemu_get_devcfg_value_hex("DEBUG_FUNC_RESET_PC", &enable_dbg.reset_pc);
	for (core_id = 0; core_id < core_id_num; core_id++) {
		cpu_set_core_pc(core_id, enable_dbg.reset_pc);
	}

	virtual_cpu.cpu_freq = DEFAULT_CPU_FREQ; /* 100MHz */
	(void)cpuemu_get_devcfg_value("DEVICE_CPU_FREQ", &virtual_cpu.cpu_freq);

	(void)cpuemu_get_devcfg_value("DEBUG_FUNC_ENABLE_SKIP_CLOCK", (uint32*)&cpuemu_dev_clock.enable_skip);
	cpuemu_set_debug_romdata();

	if (cpuemu_cui_mode() == TRUE) {
		do_cpu_run = cpuemu_thread_run_dbg;
	}
	else {
		do_cpu_run = cpuemu_thread_run_nodbg;
	}
	uint64 end_clock = cpuemu_get_cpu_end_clock();
	uint64 *clockp = &cpuemu_dev_clock.clock;
	std_bool enable_skip = cpuemu_dev_clock.enable_skip;

	while (TRUE) {
		if ((*clockp)>= end_clock) {
			dbg_log_sync();
			athrill_hakoniwa_finalize();
			std::cout << "EXIT for timeout(" << cpuemu_dev_clock.clock << ").\n";
			exit(1);
		}
		is_halt = do_cpu_run(core_id_num);

		if ((enable_skip == TRUE) && (is_halt == TRUE)
				&& (cpuemu_dev_clock.can_skip_clock == TRUE)
				&& (cpuemu_dev_clock.min_intr_interval != DEVICE_CLOCK_MAX_INTERVAL)) {
			/*
			 * CPU is halted: jump to the next device interrupt.
			 */
			cpuemu_sync_skipped_clock();
			cpuemu_dev_clock.clock += cpuemu_dev_clock.min_intr_interval;
		}
		else {
			cpuemu_dev_clock.clock += 1U;
		}
	}
	return NULL;
}

uint64 cpuemu_get_clock(void) { return cpuemu_dev_clock.clock; }
uint32 cpuemu_get_cpu_frequency(void) { return virtual_cpu.cpu_freq; }



typedef struct {
	TokenStringType	folder_path;
	TokenStringType rx_path;
	TokenStringType tx_path;
} CpuEmuFifoFileType;

static const TokenStringType fifo_tx_string = {
		2,
		{ 'T', 'X', '\0' },
};
static const TokenStringType fifo_rx_string = {
		2,
		{ 'R', 'X', '\0' },
};


static CpuEmuFifoFileType cpuemu_fifo_file;
static FileType cpuemu_fifocfg;

static Std_ReturnType parse_fifopath(char *buffer, uint32 len)
{
	Std_ReturnType err;
	TokenContainerType token_container;

	err = token_split(&token_container, (uint8*)buffer, len);
	if (err != STD_E_OK) {
		goto errdone;
	}
	err = STD_E_INVALID;
	if (token_container.num != 2) {
		goto errdone;
	}
	if (token_container.array[0].type != TOKEN_TYPE_STRING) {
		goto errdone;
	}
	else if (token_container.array[1].type != TOKEN_TYPE_STRING) {
		goto errdone;
	}

	if (token_strcmp(&fifo_tx_string, &token_container.array[0].body.str) == TRUE) {
		if (cpuemu_fifo_file.tx_path.len > 0) {
			std::cout << "ERROR: INVALID parameter number of TX >= 2\n";
			return STD_E_INVALID;
		}
		cpuemu_fifo_file.tx_path = cpuemu_fifo_file.folder_path;
		if (token_merge(&cpuemu_fifo_file.tx_path, &token_container.array[1].body.str) == FALSE) {
			std::cout << "ERROR: INVALID filename is too long: " << token_container.array[1].body.str.str << '\n';
			return STD_E_INVALID;
		}
	}
	else if (token_strcmp(&fifo_rx_string, &token_container.array[0].body.str) == TRUE) {
		if (cpuemu_fifo_file.rx_path.len > 0) {
			std::cout << "ERROR: INVALID parameter number of RX >= 2\n";
			return STD_E_INVALID;
		}
		cpuemu_fifo_file.rx_path = cpuemu_fifo_file.folder_path;
		if (token_merge(&cpuemu_fifo_file.rx_path, &token_container.array[1].body.str) == FALSE) {
			std::cout << "ERROR: INVALID filename is too long: " << token_container.array[1].body.str.str << '\n';
			return STD_E_INVALID;
		}
	}

	return STD_E_OK;
errdone:
	std::cout << "ERROR: Invalid parameter. Format should b {TX|RX} <fifo name>\n";
	return err;
}

/*
 * 出力
 * ・fifoファイル配置フォルダパス
 * ・tx fifo ファイル配置パス
 * ・rx fifo ファイル配置パス
 */
Std_ReturnType cpuemu_set_comm_fifocfg(const char* fifocfg)
{
	uint32 len;
	Std_ReturnType err;
	char buffer[4096];
	std_bool ret;

	ret = token_string_set(&cpuemu_fifocfg.filepath, fifocfg);
	if (ret == FALSE) {
		return STD_E_INVALID;
	}

	ret = file_ropen(&cpuemu_fifocfg);
	if (ret == FALSE) {
		return STD_E_NOENT;
	}

	//fifoファイル配置フォルダパス
	len = file_get_parent_folder_pathlen(fifocfg);
	memcpy(cpuemu_fifo_file.folder_path.str, fifocfg, len);
	cpuemu_fifo_file.folder_path.str[len] = '\0';
	cpuemu_fifo_file.folder_path.len = len;

	err = STD_E_INVALID;
	//fifo ファイル配置パス取得(1回目)
	len = file_getline(&cpuemu_fifocfg, buffer, 4096);
	if (len > 0) {
		err = parse_fifopath(buffer, len);
		if (err != STD_E_OK) {
			goto errdone;
		}
	}
	else {
		std::cout << "ERROR: can not found data on " << fifocfg << "...\n";
		goto errdone;
	}

	//fifo ファイル配置パス取得(2回目)
	len = file_getline(&cpuemu_fifocfg, buffer, 4096);
	if (len > 0) {
		err = parse_fifopath(buffer, len);
		if (err != STD_E_OK) {
			goto errdone;
		}
	}
	else {
		std::cout << "ERROR: can not found data on " << fifocfg << "...\n";
		goto errdone;
	}

	err = STD_E_INVALID;
	if (file_exist(cpuemu_get_comm_rx_fifo()) == FALSE) {
		std::cout << "ERROR: can not found fifo file " << cpuemu_get_comm_rx_fifo() << "...\n";
		goto errdone;
	}
	if (file_exist(cpuemu_get_comm_tx_fifo()) == FALSE) {
		std::cout << "ERROR: can not found fifo file " << cpuemu_get_comm_rx_fifo() << "...\n";
		goto errdone;
	}

	std::cout << "RX fifo:" << cpuemu_get_comm_rx_fifo() << '\n';
	std::cout << "TX fifo:" << cpuemu_get_comm_tx_fifo() << '\n';

	file_close(&cpuemu_fifocfg);
	return STD_E_OK;
errdone:
	cpuemu_fifo_file.tx_path.len = 0;
	cpuemu_fifo_file.rx_path.len = 0;
	file_close(&cpuemu_fifocfg);
	return err;
}

const char* cpuemu_get_comm_rx_fifo(void)
{
	if (cpuemu_fifo_file.rx_path.len > 0) {
		return (const char*)cpuemu_fifo_file.rx_path.str;
	}
	else {
		return NULL;
	}
}

const char* cpuemu_get_comm_tx_fifo(void)
{
	if (cpuemu_fifo_file.tx_path.len > 0) {
		return (const char*)cpuemu_fifo_file.tx_path.str;
	}
	else {
		return NULL;
	}
}

#define CPUEMU_DEVCFG_PARAM_MAXNUM	128
typedef struct {
	uint32			param_num;
	struct {
		TokenValueType	key;
		TokenValueType	value;
	} param[CPUEMU_DEVCFG_PARAM_MAXNUM];
} CpuEmuDevCfgType;

static CpuEmuDevCfgType cpuemu_devcfg;
static char dvcfg_buffer[4096];
static TokenContainerType devcfg_token_container;
static FileType devcfg_file;

Std_ReturnType cpuemu_load_devcfg(const char *path)
{
	Std_ReturnType err = STD_E_OK;
	uint32 len;
	std_bool ret;

	cpuemu_devcfg.param_num = 0;

	ret = token_string_set(&devcfg_file.filepath, path);
	if (ret == FALSE) {
		return STD_E_INVALID;
	}
	ret = file_ropen(&devcfg_file);
	if (ret == FALSE) {
		return STD_E_NOENT;
	}
	while (TRUE) {
		err = STD_E_INVALID;

		len = file_getline(&devcfg_file, dvcfg_buffer, 4096);
		if (len <= 0) {
			break;
		}

		err = token_split(&devcfg_token_container, (uint8*)dvcfg_buffer, len);
		if (err != STD_E_OK) {
			std::cout << "ERROR: can not parse data on " << path << "...\n";
			goto errdone;
		}
		if (devcfg_token_container.num != 2) {
			std::cout << "ERROR: the token is invalid " << dvcfg_buffer << " on " << path << "...\n";
			goto errdone;
		}
		cpuemu_devcfg.param[cpuemu_devcfg.param_num].key = devcfg_token_container.array[0];
		cpuemu_devcfg.param[cpuemu_devcfg.param_num].value = devcfg_token_container.array[1];
		cpuemu_devcfg.param_num++;
		//printf("param=%s\n", devcfg_token_container.array[0].body.str.str);
		//printf("value=%s\n", devcfg_token_container.array[1].body.str.str);
	}

	file_close(&devcfg_file);
	return STD_E_OK;
errdone:
	file_close(&devcfg_file);
	return err;
}

static FileType memcfg_file;
static char memcfg_buffer[4096];
static TokenContainerType memcfg_token_container;
static void analize_memmap_arguments(const TokenContainerType *token, const MemoryAddressMapType *map, MemoryAddressType *memp)
{
	int i;
	memp->region_executable = FALSE;
	memp->region_elf_load_from_vaddr = FALSE;
	if (token->num <= 3) {
		return;
	}
	for (i = 0; i < 2; i++) {
		if (token->array[3].body.str.str[i] == 'X') {
			memp->region_executable = TRUE;
		}
		else if (token->array[3].body.str.str[i] == 'V') {
			memp->region_elf_load_from_vaddr = TRUE;
		}
	}
	return;
}
Std_ReturnType cpuemu_load_memmap(const char *path, MemoryAddressMapType *map)
{
	Std_ReturnType err = STD_E_OK;
	uint32 len;
	std_bool ret;
	MemoryAddressType *memp = NULL;

	map->ram_num = 0;
	map->rom_num = 0;
	map->dev_num = 0;
	map->ram = NULL;
	map->rom = NULL;
	map->dev = NULL;

	ret = token_string_set(&memcfg_file.filepath, path);
	if (ret == FALSE) {
		return STD_E_INVALID;
	}
	ret = file_ropen(&memcfg_file);
	if (ret == FALSE) {
		return STD_E_NOENT;
	}
	while (TRUE) {
		err = STD_E_INVALID;

		len = file_getline(&memcfg_file, memcfg_buffer, 4096);
		if (len <= 0) {
			break;
		}

		err = token_split(&memcfg_token_container, (uint8*)memcfg_buffer, len);
		if (err != STD_E_OK) {
			std::cout << "ERROR: can not parse data on " << path << "...\n";
			goto errdone;
		}
		/*
		 * <memtype>, <startaddr>, <size> [, [opt]+ ]
		 * opt = {V|X}, V= load from virtual address, X = executable region
		 */
		if ((memcfg_token_container.num != 3) && (memcfg_token_container.num != 4)) {
			std::cout << "ERROR: the token is invalid " << memcfg_buffer << " on " << path << "...\n";
			goto errdone;
		}
		if (!strcmp("ROM", (char*)memcfg_token_container.array[0].body.str.str)) {
			std::cout << "ROM";
			map->rom_num++;
			map->rom = (MemoryAddressType*)realloc(map->rom, map->rom_num * sizeof(MemoryAddressType));
			ASSERT(map->rom != NULL);
			memp = &map->rom[map->rom_num - 1];
			memp->type = MemoryAddressImplType_ROM;
			memp->size = memcfg_token_container.array[2].body.dec.value;
			memp->mmap_addr = NULL;
			analize_memmap_arguments(&memcfg_token_container, map, memp);
		}
		else if (!strcmp("RAM", (char*)memcfg_token_container.array[0].body.str.str)) {
			std::cout << "RAM";
			map->ram_num++;
			map->ram = (MemoryAddressType*)realloc(map->ram, map->ram_num * sizeof(MemoryAddressType));
			ASSERT(map->ram != NULL);
			memp = &map->ram[map->ram_num - 1];
			memp->type = MemoryAddressImplType_RAM;
			memp->size = memcfg_token_container.array[2].body.dec.value;
			memp->mmap_addr = NULL;
			analize_memmap_arguments(&memcfg_token_container, map, memp);
		}
		else if (!strcmp("MMAP", (char*)memcfg_token_container.array[0].body.str.str)) {
			if (osdep_supports_mapped_memory() == FALSE) {
				std::cout << "WARNING: unknown memory type=" << (char*)memcfg_token_container.array[0].body.str.str << '\n';
				continue;
			}
			map->ram_num++;
			map->ram = (MemoryAddressType*)realloc(map->ram, map->ram_num * sizeof(MemoryAddressType));
			ASSERT(map->ram != NULL);
			memp = &map->ram[map->ram_num - 1];
			memp->type = MemoryAddressImplType_MMAP;
			{
				char* filepath;
				OsdepMappedFile mapped_file;
				AthrillDeviceMmapInfoType info;

				cpuemu_env_parse_devcfg_string(&memcfg_token_container.array[2].body.str);
				filepath = (char*)memcfg_token_container.array[2].body.str.str;
				//printf("mmap path=%s\n", filepath);
				err = osdep_open_mapped_file(filepath, &mapped_file);
				if (err != STD_E_OK) {
					std::cout << "can not open mmapfile:" << filepath << " err=" << mapped_file.error_number << '\n';
					ASSERT(err == STD_E_OK);
				}
				memp->size = (uint32)(((mapped_file.file_size + 8191) / 8192) * 8);
				if (memp->size == 0) {
					memp->size = 8;
				}
				err = osdep_map_open_file(&mapped_file, memp->size * 1024U);
				ASSERT(err == STD_E_OK);
				memp->mmap_addr = mapped_file.address;
				std::cout << "MMAP(" << filepath << " filesize=" << mapped_file.file_size << ')';
				analize_memmap_arguments(&memcfg_token_container, map, memp);
				info.fd = mapped_file.fd;
				info.addr = CAST_UINT32_TO_ADDR(memcfg_token_container.array[1].body.hex.value);
				athrill_device_set_mmap_info(&info);
			}
		}
		else if (!strcmp("MALLOC", (char*)memcfg_token_container.array[0].body.str.str)) {
			if (osdep_supports_mapped_memory() == FALSE) {
				std::cout << "WARNING: unknown memory type=" << (char*)memcfg_token_container.array[0].body.str.str << '\n';
				continue;
			}
			map->ram_num++;
			map->ram = (MemoryAddressType*)realloc(map->ram, map->ram_num * sizeof(MemoryAddressType));
			ASSERT(map->ram != NULL);
			memp = &map->ram[map->ram_num - 1];
			memp->type = MemoryAddressImplType_MALLOC;
			memp->size = memcfg_token_container.array[2].body.dec.value;
			memp->mmap_addr = NULL;
			if ((memp->size % (MPU_MALLOC_REGION_UNIT_SIZE * MPU_MALLOC_REGION_UNIT_GROUP_NUM)) != 0) {
				err = STD_E_INVALID;
				std::cout << "ERROR: Invalid MALLOC size(" << memp->size << "). The size must be multiples of 10MB.\n";
				goto errdone;
			}
			analize_memmap_arguments(&memcfg_token_container, map, memp);
			std::cout << "MALLOC";
		}
#ifdef EXDEV_ENABLE
		else if (!strcmp("DEV", (char*)memcfg_token_container.array[0].body.str.str)) {
			char error_message[512];
			char *filepath = (char*)memcfg_token_container.array[2].body.str.str;
			AthrillSharedLibraryHandle handle = athrill_shared_library_open(
				filepath, error_message, sizeof(error_message));
			AthrillExDeviceHeaderType *ext_dev_header;

			if (handle == NULL) {
				std::cout << "ERROR: Can not load shared library " << filepath << " reason=" << error_message << '\n';
				continue;
			}
			ext_dev_header = (AthrillExDeviceHeaderType*)athrill_shared_library_symbol(
				handle, "athrill_ex_device", error_message, sizeof(error_message));
			if (ext_dev_header == NULL) {
				std::cout << "ERROR: Can not find symbol(athrill_ex_device) on " << filepath
					<< " reason=" << error_message << '\n';
				athrill_shared_library_close(handle);
				continue;
			}
			if (ext_dev_header->magicno != ATHRILL_EXTERNAL_DEVICE_MAGICNO) {
				std::cout << "ERROR: magicno is invalid(0x" << std::hex << ext_dev_header->magicno << std::dec << ") on " << filepath << '\n';
				athrill_shared_library_close(handle);
				continue;
			}
			if (ext_dev_header->version != ATHRILL_EXTERNAL_DEVICE_VERSION) {
				std::cout << "ERROR: version is invalid(0x" << std::hex << ext_dev_header->version << std::dec << ") on " << filepath << '\n';
				athrill_shared_library_close(handle);
				continue;
			}
			if (ext_dev_header->memory_size <= 0) {
				std::cout << "ERROR: memory size is invalid(" << ext_dev_header->memory_size << ") on " << filepath << '\n';
				athrill_shared_library_close(handle);
				continue;
			}
			map->dev_num++;
			map->dev = (MemoryAddressType*)realloc(map->dev, map->dev_num * sizeof(MemoryAddressType));
			ASSERT(map->dev != NULL);
			memp = &map->dev[map->dev_num - 1];
			memp->type = MemoryAddressImplType_DEV;
			memp->extdev_handle = handle;
			memp->size = (uint32)ext_dev_header->memory_size;
			memp->mmap_addr = NULL;
			std::cout << "DEV";
		}
#endif
		else {
			std::cout << "WARNING: unknown memory type=" << (char*)memcfg_token_container.array[0].body.str.str << '\n';
			continue;
		}
		memp->start = (AthrillAddrType)memcfg_token_container.array[1].body.hex.value;
		std::cout << " : START=0x" << std::hex << memp->start << std::dec << " SIZE=" << memp->size << '\n';
	}

	file_close(&memcfg_file);
	return STD_E_OK;
errdone:
	file_close(&memcfg_file);
	return err;
}


Std_ReturnType cpuemu_get_devcfg_value(const char* key, uint32 *value)
{
	int i;
	TokenStringType token;

	token.len = strlen(key);
	memcpy(token.str, key, token.len);
	token.str[token.len] = '\0';

	for (i = 0; i < cpuemu_devcfg.param_num; i++) {
		if (cpuemu_devcfg.param[i].value.type != TOKEN_TYPE_VALUE_DEC) {
			continue;
		}
		if (token_strcmp(&cpuemu_devcfg.param[i].key.body.str, &token) == FALSE) {
			continue;
		}
		*value = cpuemu_devcfg.param[i].value.body.dec.value;
		return STD_E_OK;
	}
	return STD_E_NOENT;
}

Std_ReturnType cpuemu_get_devcfg_value_hex(const char* key, uint32 *value)
{
	int i;
	TokenStringType token;

	token.len = strlen(key);
	memcpy(token.str, key, token.len);
	token.str[token.len] = '\0';

	for (i = 0; i < cpuemu_devcfg.param_num; i++) {
		if (cpuemu_devcfg.param[i].value.type != TOKEN_TYPE_VALUE_HEX) {
			continue;
		}
		if (token_strcmp(&cpuemu_devcfg.param[i].key.body.str, &token) == FALSE) {
			continue;
		}
		*value = (uint32)cpuemu_devcfg.param[i].value.body.hex.value;
		return STD_E_OK;
	}
	return STD_E_NOENT;
}

static void cpuemu_env_parse_devcfg_string(TokenStringType* strp)
{
	static char env_name[TOKEN_STRING_MAX_SIZE];
	static char out_name[TOKEN_STRING_MAX_SIZE];
	const char *start = strchr((const char*)strp->str, '{');
	const char *end = strchr((const char*)strp->str, '}');
	if ((start == NULL) || (end == NULL)) {
		return;
	}
	int len = ((int)(end - start) - 1);
	if (len == 0) {
		return;
	}
	memset(env_name, 0, TOKEN_STRING_MAX_SIZE);
	memcpy(env_name, (start + 1), len);

	//printf("%s\n", env_name);
	char *ep = getenv(env_name);
	if (ep == NULL) {
		return;
	}
	//printf("ep = %s\n", ep);
	memset(out_name, 0, TOKEN_STRING_MAX_SIZE);
	len = snprintf(out_name, TOKEN_STRING_MAX_SIZE, "%s%s", ep, (end + 1));
	out_name[len] = '\0';
	len++;
	//printf("out_name=%s len=%d\n", out_name, len);
	memcpy(strp->str, out_name, len);
	strp->len = len;

	return;
}

Std_ReturnType cpuemu_get_devcfg_string(const char* key, char **value)
{
	int i;
	TokenStringType token;

	token.len = strlen(key);
	memcpy(token.str, key, token.len);
	token.str[token.len] = '\0';

	for (i = 0; i < cpuemu_devcfg.param_num; i++) {
		if (cpuemu_devcfg.param[i].value.type != TOKEN_TYPE_STRING) {
			continue;
		}
		if (token_strcmp(&cpuemu_devcfg.param[i].key.body.str, &token) == FALSE) {
			continue;
		}
		cpuemu_env_parse_devcfg_string(&cpuemu_devcfg.param[i].value.body.str);
		*value = (char*)cpuemu_devcfg.param[i].value.body.str.str;
		std::cout << key << " = " << *value << '\n';
		return STD_E_OK;
	}
	return STD_E_NOENT;
}

void cpuemu_raise_intr(uint32 intno)
{
	(void)intc_raise_intr(intno);
	return;
}
