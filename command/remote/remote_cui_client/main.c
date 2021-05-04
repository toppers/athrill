#include "udp/udp_comm.h"
#include "target/target_os_api.h"
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include "cpuemu_config.h"

typedef struct {
	uint16	athrill_listen_port;
	uint16	remote_client_listen_port;
	bool    is_verbose;
	bool    is_help;
	bool    is_parse_error;
} CmdOptionType;

CmdOptionType parse_option(int argc, const char* argv[]);

static int cmd_buffer_len = 0;
static char cmd_buffer[4096];

int main(int argc, const char* argv[])
{
	int i;
	UdpCommConfigType config;
	UdpCommType comm;
	int len;

	CmdOptionType opt = parse_option(argc, argv);

	if (argc - optind < 1 || opt.is_help || opt.is_parse_error) {
		printf("Usage: athrill_remote [OPTION]... COMMAND\n");
		printf("       -a, --athrill-listen-port<port no>       : set Athrill listen port.\n");
		printf("       -r, --remote-client-listen-port<port no> : set athrill_remote listen port.\n");
		printf("       -v, --verbose                            : print verbose message.\n");
		printf("       -h, --help                               : print this message.\n");
		return 1;
	}

	if (opt.is_verbose) {
		printf("athrill_listen_port: %d\n", opt.athrill_listen_port);
		printf("remote_client_listen_port: %d\n", opt.remote_client_listen_port);
	}

	for (i = optind; i < argc; i++) {
		len = strlen(argv[i]);
		if ((cmd_buffer_len + (len + 2)) > UDP_BUFFER_LEN) {
			printf("argument length is too large.len=%d\n", len);
			return 1;
		}
		memcpy(&cmd_buffer[cmd_buffer_len], argv[i], len);
		cmd_buffer_len += (len + 1);
		cmd_buffer[cmd_buffer_len] = ' ';
	}

	config.server_port = opt.remote_client_listen_port;
	config.client_port = opt.athrill_listen_port;
	config.is_wait = TRUE;

	comm.write_data.len = cmd_buffer_len;
	memcpy(comm.write_data.buffer, cmd_buffer, cmd_buffer_len);
	cmd_buffer[cmd_buffer_len] = '\0';

#ifdef OS_LINUX
#else
	if (winsock_init() < 0) {
		printf("ERROR:winsock_init failed.\n");
		return 1;
	}
#endif

	if (udp_comm_create(&config, &comm) != STD_E_OK) {
		printf("ERROR:udp_comm_create failed.\n");
		return 1;
	}

	if (udp_comm_write(&comm) != STD_E_OK) {
		printf("ERROR:udp_comm_write failed.\n");
		return 1;
	}
	if (udp_comm_read(&comm) != STD_E_OK) {
		printf("ERROR:udp_comm_read failed.\n");
		return 1;
	}
#ifdef OS_LINUX
#else
	winsock_fini();
#endif

	printf("%s", comm.read_data.buffer);
	return 0;
}

CmdOptionType parse_option(int argc, const char* argv[]) {
	CmdOptionType options = {
		.athrill_listen_port = CPUEMU_CONFIG_CUI_CLIENT_PORTNO,
		.remote_client_listen_port = CPUEMU_CONFIG_CUI_EMULATOR_PORTNO,
		.is_verbose = FALSE,
		.is_help = FALSE,
		.is_parse_error = FALSE,
	};

	struct option longopts[] = {
		{ "remote-client-listen-port", required_argument, NULL, 'r' },
		{ "athrill-listen-port",       required_argument, NULL, 'a' },
		{ "verbose",                   no_argument,       NULL, 'v' },
		{ "help",                      no_argument,       NULL, 'h' },
		{ 0,                           0,                 0,     0  },
	};

	int opt, longindex;
	while ((opt = getopt_long(argc, (char**)argv, "a:r:vh", longopts, &longindex)) != -1) {
		switch (opt) {
		case 'r':
			options.remote_client_listen_port = atoi(optarg);
			break;
		case 'a':
			options.athrill_listen_port = atoi(optarg);
			break;
		case 'v':
			options.is_verbose = TRUE;
			break;
		case 'h':
			options.is_help = TRUE;
			break;
		default:
			options.is_parse_error = TRUE;
			break;
		}
	}

	return options;

}

