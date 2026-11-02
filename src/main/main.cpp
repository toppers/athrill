#include <iomanip>
#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "front/parser/dbg_parser.h"
#include "loader/loader.h"
#include "option/option.h"
#include "cpu_control/dbg_cpu_control.h"
#include "cpu_control/dbg_cpu_thread_control.h"
#include "cpuemu_config.h"
#include "cpuemu_ops.h"
#include "cui/cui_ops.h"
#include "cui/stdio/cui_ops_stdio.h"
#include "cui/udp/cui_ops_udp.h"
#include "file_address_mapping.h"
#include "osdep.h"
#include "target/target_os_api.h"
#include "athrill_exdev_header.h"
#include "athrill_memory.h"
#include "athrill_device.h"
}

/*
 * version: X.Y.Z
 *  X: generation
 *  Y: function
 *  Z: bug fix, small changes
 */
#define ATHRILL_CORE_VERSION "1.2.0"

#ifndef ATHRILL_TARGET_ARCH
#define ATHRILL_TARGET_ARCH "UNKNOWN"
#define ATHRILL_TARGET_VERSION "0.0.0"
#endif

AthrillMemHeadType athrill_mem_head = { NULL, NULL };

static FILE *save_operation_fp = NULL;

static void load_cui_operation(void)
{
	DbgCmdExecutorType *res;
	char *filename;
	char buffer[1025];
	size_t len;

	Std_ReturnType err = cpuemu_get_devcfg_string("DEBUG_FUNC_OPLOG", &filename);
	if (err != STD_E_OK) {
		return;
	}
	save_operation_fp = fopen(filename, "a+");
	if (save_operation_fp == NULL) {
		std::cout << "ERROR: can not open oplog file " << filename << '\n';
		return;
	}
	while (TRUE) {
		if (fgets(buffer, sizeof(buffer), save_operation_fp) == NULL) {
			break;
		}
		len = strlen(buffer);
		if ((len > 0) && (buffer[len - 1] == '\n')) {
			buffer[--len] = '\0';
		}
		res = dbg_parse((uint8*)buffer, (uint32)len);

		if (res != NULL) {
			res->run(res);
		}
	}
	return;
}
static void save_cui_operation(const char* op)
{
	if (save_operation_fp == NULL) {
		return;
	}
	fprintf(save_operation_fp, "%s\n", op);
	fflush(save_operation_fp);
	return;
}

static void do_cui(void)
{
	DbgCmdExecutorType *res;
	std_bool is_dbgmode;
	char buffer[1025];
	int len;

	load_cui_operation();

	while (TRUE) {
		is_dbgmode = cpuctrl_is_debug_mode();
		std::cout << ((is_dbgmode == TRUE) ? "[DBG>" : "[CPU>");
		fflush(stdout);
retry:
		len = cui_getline(buffer, 1024);
		if (len < 0) {
			cui_close();
			target_os_api_sleep(1000);
			goto retry;
		}
		buffer[len] = '\0';
		res = dbg_parse((uint8*)buffer, (uint32)len);

		if (res != NULL) {
			res->result_ok = FALSE;
			res->run(res);
			if (res->result_ok == TRUE) {
				save_cui_operation((const char*)res->original_str);
			}
		}
	}
}

/*
 * コマンドオプション仕様
 * -i		インタラクションモード
 * 	・あり：インタラクションモード
 * 	・なし：バックグラウンド実行モード
 * -r(インタラクションモードのみ有効)
 * 	・あり：リモートモード
 * 	・なし：直接モード
 * -t<time>	終了時間(単位：clock)
 * 	・あり：終了時間
 * 	・なし：無制限
 * -b	入力ファイル形式
 * 	・あり：バイナリデータ
 * 	・なし：ELFファイル
 * -p<fifo config file path>
 * 	・あり：対抗ECUとの通信あり
 * 	・なし：シングルECU構成
 */
int main(int argc, const char *argv[])
{
	Std_ReturnType err;
	CmdOptionType *opt;
	MemoryAddressMapType memmap;
	uint32 entry_addr = 0U;
	memset(&memmap, 0, sizeof(MemoryAddressMapType));

	if (argc == 1) {
		std::cout << "Athrill is licensed under the TOPPERS License Agreement (http://www.toppers.jp/en/license.html).\n";
		std::cout << "ARCH:" << ATHRILL_TARGET_ARCH << " (VERSION CORE:" << ATHRILL_CORE_VERSION << " TARGET:" << ATHRILL_TARGET_VERSION << ")\n";
		std::cout << "DEVICE:magicno=0x" << std::hex << ATHRILL_EXTERNAL_DEVICE_MAGICNO << " version=0x" << ATHRILL_EXTERNAL_DEVICE_VERSION << std::dec << "\n\n";

		std::cout << "Usage:athrill -c<core num> -m <memory config file> [OPTION]... <load_file>\n";
		std::cout << ' ' << std::left << std::setw(30) << "-c" << std::right << " : set core num. if -c is not set, core num = 2.\n";
		std::cout << ' ' << std::left << std::setw(30) << "-i" << std::right << " : execute on the interaction mode. if -i is not set, execute on the background mode.\n";
		std::cout << ' ' << std::left << std::setw(30) << "-r" << std::right << " : execute on the remote mode. this option is valid on the interaction mode.\n";
		std::cout << ' ' << std::left << std::setw(30) << "-t<timeout>" << std::right << " : set program end time using <timeout> clocks. this option is valid on the background mode.\n";
		std::cout << ' ' << std::left << std::setw(30) << "-m<memory config file>" << std::right << " : set athrill memory configuration. rom, ram region is configured on your system.\n";
		//printf(" %-30s : set communication path with an another emulator.\n", "-p<fifo config file>");
		std::cout << ' ' << std::left << std::setw(30) << "-d<device config file>" << std::right << " : set device parameter.\n";
		return 0;
	}

	winsock_init();
#ifdef EXDEV_ENABLE
	(void)atexit(athrill_device_cleanup);
	osdep_install_terminate_handler(athrill_device_cleanup);
#else
	osdep_install_terminate_handler(NULL);
#endif

	opt = parse_args(argc, argv);
	if (opt == NULL) {
		return 1;
	}
	std::cout << "core id num=" << opt->core_id_num << '\n';
	if (opt->fifocfgpath != NULL) {
		err = cpuemu_set_comm_fifocfg(opt->fifocfgpath);
		if (err != STD_E_OK) {
			return -1;
		}
	}
	if (opt->devcfgpath != NULL) {
		err = cpuemu_load_devcfg(opt->devcfgpath);
		if (err != STD_E_OK) {
			return -1;
		}
	}
	if (opt->memfilepath != NULL) {
		err = cpuemu_load_memmap(opt->memfilepath, &memmap);
		if (err != STD_E_OK) {
			return -1;
		}
	}

	if (opt->is_binary_data) {
		binary_load((uint8*)opt->load_file.buffer, 0U, opt->load_file.size);
	}
	else {
		elf_load((uint8*)opt->load_file.buffer, &memmap, &entry_addr);
		if (cpuemu_symbol_set() != STD_E_OK) {
			return -1;
		}
		file_address_mapping_init();
	}

	cpuemu_set_entry_addr(entry_addr);
	if (opt->is_interaction == TRUE) {
		if (opt->is_remote == TRUE) {
			uint32 athrill_listen_port = CPUEMU_CONFIG_CUI_EMULATOR_PORTNO;
			uint32 client_listen_port = CPUEMU_CONFIG_CUI_CLIENT_PORTNO;
			(void)cpuemu_get_devcfg_value("DEBUG_FUNC_REMOTE_ATHRILL_LISTEN_PORT_NO", &athrill_listen_port);
			(void)cpuemu_get_devcfg_value("DEBUG_FUNC_REMOTE_CLIENT_LISTEN_PORT_NO", &client_listen_port);
			cui_ops_udp_init((uint16)client_listen_port, (uint16)athrill_listen_port);
		}
		else {
			cui_ops_stdio_init();
		}

		cpuemu_init(cpuemu_thread_run, opt);
		do_cui();
	}
	else {
		cpuemu_init(NULL, opt);
		cpuemu_set_cpu_end_clock(opt->timeout);
		(void)cpuemu_thread_run(NULL);
	}

	return 0;
}
