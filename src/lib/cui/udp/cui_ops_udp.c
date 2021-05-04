#include "cui/cui_ops.h"
#include "cui/udp/cui_ops_udp.h"
#include "udp/udp_comm.h"
#include "cpuemu_config.h"
#include <stdio.h>

typedef struct {
	UdpCommConfigType	config;
	UdpCommType			comm;
	FileOpType 			op;
} UdpFileOpType;

static void cui_close_udp(void);
static int  cui_getline_udp(char *line, int size);
static void cui_write_udp(char *line, int size);

static UdpFileOpType cui_fileop_udp;

void cui_ops_udp_init(uint16 server_port, uint16 client_port)
{
	Std_ReturnType err;

	UdpFileOpType cui_fileop_udp_tmp = {
		.config = {
				.server_port = server_port,
				.client_port = client_port,
				.is_wait = TRUE,
		},
		.op = {
				.cui_getline = cui_getline_udp,
				.cui_write = cui_write_udp,
				.cui_close = cui_close_udp,
		},

	};

	cui_fileop_udp = cui_fileop_udp_tmp;

	printf("REMOTE:athrill listen port %u\n", cui_fileop_udp.config.server_port);
	printf("REMOTE:client listen port %u\n", cui_fileop_udp.config.client_port);

	err = udp_comm_create(&cui_fileop_udp.config, &cui_fileop_udp.comm);
	if (err != STD_E_OK) {
		printf("ERROR:internal error:udp_server_create()\n");
		exit(1);
	}

	(void)cui_fileop_register(&cui_fileop_udp.op);
	return;
}

static void cui_close_udp(void)
{
	Std_ReturnType err;

	udp_server_delete(&cui_fileop_udp.comm);

	err = udp_comm_create(&cui_fileop_udp.config, &cui_fileop_udp.comm);
	if (err != STD_E_OK) {
		printf("ERROR:internal error:udp_server_create()\n");
		exit(1);
	}
	return;
}

static int  cui_getline_udp(char *line, int size)
{
	Std_ReturnType err;

	//printf("cui_getline_udp:enter\n");
	err = udp_comm_read(&cui_fileop_udp.comm);
	if (err != STD_E_OK) {
		printf("ERROR:internal error:udp_server_read()\n");
		return -1;
	}
	if (cui_fileop_udp.comm.read_data.len > size) {
		printf("ERROR:internal error:cui_getline_udp():recv size is too large\n");
		return -1;
	}
	memcpy(line, cui_fileop_udp.comm.read_data.buffer, cui_fileop_udp.comm.read_data.len);
	line[cui_fileop_udp.comm.read_data.len] = '\0';
	//printf("cui_getline_udp:exit:%s\n", line);
	return cui_fileop_udp.comm.read_data.len;
}

static void cui_write_udp(char *line, int size)
{
	Std_ReturnType err;

	if (size > UDP_BUFFER_LEN) {
		return;
	}
	cui_fileop_udp.comm.write_data.len = size;
	memcpy(cui_fileop_udp.comm.write_data.buffer, line, size);

	err = udp_comm_write(&cui_fileop_udp.comm);
	if (err != STD_E_OK) {
		printf("ERROR:internal error:udp_server_write()\n");
		return;
	}
	return;
}
