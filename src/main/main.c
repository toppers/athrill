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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "target/target_os_api.h"
#include <sched.h>
#include <limits.h>
#include "athrill_exdev_header.h"
#ifdef OS_LINUX
#include <signal.h>
#endif
#include "athrill_memory.h"
#include "athrill_device.h"

/*
 * version: X.Y.Z
 *  X: generation
 *  Y: function
 *  Z: bug fix, small changes
 */
#define ATHRILL_CORE_VERSION "1.1.2"

#ifndef ATHRILL_TARGET_ARCH
#define ATHRILL_TARGET_ARCH "UNKNOWN"
#define ATHRILL_TARGET_VERSION "0.0.0"
#endif

AthrillMemHeadType athrill_mem_head = { NULL, NULL };

static FILE *save_operation_fp = NULL;

static void load_cui_operation(void)
{
#ifdef 	OS_LINUX	/* Windows not support */
	DbgCmdExecutorType *res;
	char *filename;
	size_t buffer_size = 1024;
	char buffer[1025];
	char *lineptr = buffer;
	ssize_t len;

	Std_ReturnType err = cpuemu_get_devcfg_string("DEBUG_FUNC_OPLOG", &filename);
	if (err != STD_E_OK) {
		return;
	}
	save_operation_fp = fopen(filename, "a+");
	if (save_operation_fp == NULL) {
		printf("ERROR: can not open oplog file %s\n", filename);
		return;
	}
	while (TRUE) {
		len = getline(&lineptr, &buffer_size, save_operation_fp);
		if (len < 0) {
			break;
		}
		buffer[len] = '\0';
		res = dbg_parse((uint8*)buffer, (uint32)len);

		if (res != NULL) {
			res->run(res);
		}
	}
#endif	/* OS_LINUX	 */
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
	bool is_dbgmode;
	char buffer[1025];
	int len;

	load_cui_operation();

	while (TRUE) {
		is_dbgmode = cpuctrl_is_debug_mode();
		printf("%s", (is_dbgmode == TRUE) ? "[DBG>" : "[CPU>");
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

#ifdef OS_LINUX
static void athrill_sigterm_handler(int arg)
{
	athrill_device_cleanup();
	sync();
	sync();
	sync();
	sync();
	sync();
	exit(0);
	return;
}
#endif


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
	memset(&memmap, 0, sizeof(MemoryAddressMapType));

	if (argc == 1) {
		printf("Athrill is licensed under the TOPPERS License Agreement (http://www.toppers.jp/en/license.html).\n");
		printf("ARCH:%s (VERSION CORE:%s TARGET:%s)\n", ATHRILL_TARGET_ARCH, ATHRILL_CORE_VERSION, ATHRILL_TARGET_VERSION);
		printf("DEVICE:magicno=0x%x version=0x%x\n\n", ATHRILL_EXTERNAL_DEVICE_MAGICNO, ATHRILL_EXTERNAL_DEVICE_VERSION);

		printf("Usage:%s -c<core num> -m <memory config file> [OPTION]... <load_file>\n", "athrill");
		printf(" %-30s : set core num. if -c is not set, core num = 2.\n", "-c");
		printf(" %-30s : execute on the interaction mode. if -i is not set, execute on the background mode.\n", "-i");
		printf(" %-30s : execute on the remote mode. this option is valid on the interaction mode.\n", "-r");
		printf(" %-30s : set program end time using <timeout> clocks. this option is valid on the background mode.\n", "-t<timeout>");
		printf(" %-30s : set athrill memory configuration. rom, ram region is configured on your system.\n", "-m<memory config file>");
		//printf(" %-30s : set communication path with an another emulator.\n", "-p<fifo config file>");
		printf(" %-30s : set device parameter.\n", "-d<device config file>");
		return 0;
	}

	winsock_init();
#if 0
	struct sched_param sp;

	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	int ret = sched_setscheduler(0, SCHED_FIFO, &sp);
	if (ret) {
		perror("sched_setscheduler");
		return 1;
	}
#endif
#ifdef OS_LINUX
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, athrill_sigterm_handler);
#endif

	opt = parse_args(argc, argv);
	if (opt == NULL) {
		return 1;
	}
	printf("core id num=%u\n", opt->core_id_num);
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
		elf_load((uint8*)opt->load_file.buffer, &memmap);
		if (cpuemu_symbol_set() != STD_E_OK) {
			return -1;
		}
		file_address_mapping_init();
	}

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
