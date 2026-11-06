#include "gdb_rsp.h"
#include "gdb_rsp_target.h"
#include "cpu_control/dbg_cpu_control.h"
#include "cpu_control/dbg_cpu_thread_control.h"
#include "bus.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(OS_LINUX) || defined(__APPLE__)
#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define RSP_PACKET_SIZE 8192

static int read_byte(int fd, uint8 *byte)
{
	ssize_t n = recv(fd, byte, 1, 0);
	return n == 1 ? 1 : 0;
}

static int read_packet(int fd, char *packet, size_t capacity)
{
	uint8 ch = 0;
	unsigned int checksum = 0;
	char checksum_text[3];
	unsigned int expected;
	size_t len = 0;
	char *end;

	while (read_byte(fd, &ch) && ch != '$') {}
	if (ch != '$') return 0;
	while (read_byte(fd, &ch) && ch != '#') {
		checksum = (checksum + ch) & 0xff;
		if (len + 1 >= capacity) return 0;
		packet[len++] = (char)ch;
	}
	if (ch != '#' || !read_byte(fd, (uint8 *)&checksum_text[0]) || !read_byte(fd, (uint8 *)&checksum_text[1])) return 0;
	checksum_text[2] = '\0';
	expected = (unsigned int)strtoul(checksum_text, &end, 16);
	if (*end != '\0' || expected != checksum) {
		(void)send(fd, "-", 1, 0);
		return -1;
	}
	packet[len] = '\0';
	(void)send(fd, "+", 1, 0);
	return (int)len;
}

static int send_packet(int fd, const char *packet)
{
	char frame[RSP_PACKET_SIZE + 8];
	unsigned int checksum = 0;
	size_t i, len = strlen(packet);
	if (len > RSP_PACKET_SIZE) return -1;
	frame[0] = '$';
	memcpy(frame + 1, packet, len);
	for (i = 0; i < len; i++) checksum = (checksum + (uint8)packet[i]) & 0xff;
	sprintf(frame + len + 1, "#%02x", checksum);
	return send(fd, frame, len + 4, 0) == (ssize_t)(len + 4) ? 0 : -1;
}

static int hex_value(char ch)
{
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

static int parse_hex_u32(const char *text, uint32 *value)
{
	char *end;
	unsigned long v;
	errno = 0;
	v = strtoul(text, &end, 16);
	if (text == end || *end != '\0' || errno != 0 || v > 0xffffffffUL) return -1;
	*value = (uint32)v;
	return 0;
}

static char *hex_encode(char *out, const uint8 *data, size_t len)
{
	static const char digits[] = "0123456789abcdef";
	size_t i;
	for (i = 0; i < len; i++) {
		*out++ = digits[data[i] >> 4];
		*out++ = digits[data[i] & 0xf];
	}
	*out = '\0';
	return out;
}

static int stop_reply(int fd, uint8 signal, const char *reason)
{
	char reply[96];
	snprintf(reply, sizeof(reply), "T%02xthread:1;%s", signal, reason);
	return send_packet(fd, reply);
}

static int await_stop(int fd)
{
	uint8 stop_signal = 5;
	while (TRUE) {
		fd_set rfds;
		struct timeval timeout;
		int rc;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 20000;
		rc = select(fd + 1, &rfds, NULL, NULL, &timeout);
		if (rc > 0 && FD_ISSET(fd, &rfds)) {
			uint8 ch;
			if (!read_byte(fd, &ch)) {
				/* A disconnected debugger must not leave the target running. */
				cpuctrl_set_force_break();
				cputhr_control_dbg_waitfor_cpu_stopped();
				return -1;
			}
			if (ch == 3) {
				stop_signal = 2;
				cpuctrl_set_force_break();
			}
		}
		if (cpuctrl_is_debug_mode() == TRUE) {
			cputhr_control_dbg_waitfor_cpu_stopped();
			return stop_reply(fd, stop_signal, "");
		}
	}
}

static int dispatch_packet(int fd, char *packet, int *should_close)
{
	char response[RSP_PACKET_SIZE + 1];
	uint32 address, length;
	uint8 data[4096];
	int i;
	char *sep;
	*should_close = FALSE;

	if (strncmp(packet, "qSupported", 10) == 0) return send_packet(fd, "PacketSize=2000;vContSupported+");
	if (strcmp(packet, "?" ) == 0) return stop_reply(fd, 5, "");
	if (strcmp(packet, "qAttached") == 0) return send_packet(fd, "1");
	if (strcmp(packet, "qC") == 0) return send_packet(fd, "QC1");
	if (strcmp(packet, "qfThreadInfo") == 0) return send_packet(fd, "m1");
	if (strcmp(packet, "qsThreadInfo") == 0) return send_packet(fd, "l");
	if (packet[0] == 'H') return send_packet(fd, "OK");
	if (strcmp(packet, "vCont?") == 0) return send_packet(fd, "vCont;c;s");
	if (strcmp(packet, "D") == 0) { *should_close = TRUE; return send_packet(fd, "OK"); }
	if (packet[0] == 'k') { *should_close = TRUE; return 1; }
	if (packet[0] == 'g') {
		char *out = response;
		for (uint32 reg = 0; reg < dbg_target_gdb_reg_count(); reg++) {
			uint32 size = dbg_target_gdb_reg_size(reg);
			memset(data, 0, size);
			if (dbg_target_gdb_read_reg(0, reg, data) == TRUE) out = hex_encode(out, data, size);
			else { memset(out, 'x', size * 2); out += size * 2; *out = '\0'; }
		}
		return send_packet(fd, response);
	}
	if (packet[0] == 'p') {
		uint32 regno;
		if (parse_hex_u32(packet + 1, &regno) < 0 || regno >= dbg_target_gdb_reg_count()) return send_packet(fd, "E01");
		uint32 size = dbg_target_gdb_reg_size(regno);
		if (dbg_target_gdb_read_reg(0, regno, data) == FALSE) { memset(response, 'x', size * 2); response[size * 2] = '\0'; return send_packet(fd, response); }
		hex_encode(response, data, size);
		return send_packet(fd, response);
	}
	if (packet[0] == 'P') {
		uint32 regno;
		sep = strchr(packet + 1, '=');
		if (sep == NULL) return send_packet(fd, "E01");
		*sep++ = '\0';
		if (parse_hex_u32(packet + 1, &regno) < 0 || regno >= dbg_target_gdb_reg_count()) return send_packet(fd, "E01");
		uint32 size = dbg_target_gdb_reg_size(regno);
		if (strlen(sep) != size * 2) return send_packet(fd, "E01");
		if (strspn(sep, "x") == size * 2) return send_packet(fd, "OK");
		for (i = 0; i < (int)size; i++) { int hi = hex_value(sep[i * 2]), lo = hex_value(sep[i * 2 + 1]); if (hi < 0 || lo < 0) return send_packet(fd, "E01"); data[i] = (uint8)((hi << 4) | lo); }
		return send_packet(fd, dbg_target_gdb_write_reg(0, regno, data) ? "OK" : "E02");
	}
	if (packet[0] == 'G') {
		const char *cursor = packet + 1;
		for (uint32 reg = 0; reg < dbg_target_gdb_reg_count(); reg++) {
			uint32 size = dbg_target_gdb_reg_size(reg);
			if ((size_t)(strlen(cursor)) < size * 2) return send_packet(fd, "E01");
			if (strspn(cursor, "x") >= size * 2) { cursor += size * 2; continue; }
			for (i = 0; i < (int)size; i++) { int hi = hex_value(cursor[i * 2]), lo = hex_value(cursor[i * 2 + 1]); if (hi < 0 || lo < 0) return send_packet(fd, "E01"); data[i] = (uint8)((hi << 4) | lo); }
			if (!dbg_target_gdb_write_reg(0, reg, data)) return send_packet(fd, "E02");
			cursor += size * 2;
		}
		return send_packet(fd, *cursor == '\0' ? "OK" : "E01");
	}
	if (packet[0] == 'm') {
		sep = strchr(packet + 1, ',');
		if (sep == NULL || (size_t)(sep - (packet + 1)) >= 32) return send_packet(fd, "E01");
		*sep = '\0';
		if (parse_hex_u32(packet + 1, &address) < 0 || parse_hex_u32(sep + 1, &length) < 0 || length > sizeof(data)) return send_packet(fd, "E01");
		for (i = 0; i < (int)length; i++) if (bus_get_data8(0, address + (uint32)i, &data[i]) != STD_E_OK) return send_packet(fd, "E02");
		hex_encode(response, data, length);
		return send_packet(fd, response);
	}
	if (packet[0] == 'Z' || packet[0] == 'z') {
		uint32 type, size;
		char *address_sep;
		sep = strchr(packet + 1, ',');
		if (sep == NULL) return send_packet(fd, "E01");
		address_sep = strchr(sep + 1, ',');
		if (address_sep == NULL) return send_packet(fd, "E01");
		*sep = '\0';
		*address_sep = '\0';
		type = (uint32)strtoul(packet + 1, NULL, 16);
		if (type != 0 || parse_hex_u32(sep + 1, &address) < 0 || parse_hex_u32(address_sep + 1, &size) < 0) return send_packet(fd, "");
		if (size == 0) return send_packet(fd, "E01");
		if (packet[0] == 'Z') return send_packet(fd, cpuctrl_set_break(address, BREAK_POINT_TYPE_FOREVER) ? "OK" : "E02");
		for (i = 0; i < DBG_CPU_CONTROL_BREAK_SETSIZE; i++) {
			uint32 current;
			if (cpuctrl_get_break((uint32)i, &current) && current == address) { cpuctrl_del_break((uint32)i); return send_packet(fd, "OK"); }
		}
		return send_packet(fd, "E03");
	}
	if (packet[0] == 'c' || packet[0] == 's' || strncmp(packet, "vCont;c", 7) == 0 || strncmp(packet, "vCont;s", 7) == 0) {
		std_bool stepping = packet[0] == 's' || strncmp(packet, "vCont;s", 7) == 0 || strstr(packet, ";s") != NULL;
		if (packet[1] != '\0' && packet[0] != 'v') {
			if (parse_hex_u32(packet + 1, &address) < 0) return send_packet(fd, "E01");
			dbg_target_gdb_set_pc(0, address);
		}
		if (stepping) {
			/* Match the CLI `next` path: resume in debug mode for one instruction. */
			cpuctrl_set_cont_clocks(FALSE, 0);
			cpuctrl_set_debug_mode(TRUE);
			cputhr_control_dbg_wakeup_cpu_and_wait_for_cpu_stopped();
			return stop_reply(fd, 5, "");
		}
		cpuctrl_set_cont_clocks(FALSE, 0);
		cpuctrl_set_debug_mode(FALSE);
		cputhr_control_dbg_wakeup_cpu();
		return await_stop(fd);
	}
	if (packet[0] == 'M') {
		sep = strchr(packet + 1, ',');
		if (sep == NULL) return send_packet(fd, "E01");
		*sep++ = '\0';
		char *colon = strchr(sep, ':');
		if (colon == NULL) return send_packet(fd, "E01");
		*colon++ = '\0';
		if (parse_hex_u32(packet + 1, &address) < 0 || parse_hex_u32(sep, &length) < 0 || length > sizeof(data) || strlen(colon) != length * 2) return send_packet(fd, "E01");
		for (i = 0; i < (int)length; i++) {
			int hi = hex_value(colon[i * 2]), lo = hex_value(colon[i * 2 + 1]);
			if (hi < 0 || lo < 0 || bus_put_data8(0, address + (uint32)i, (uint8)((hi << 4) | lo)) != STD_E_OK) return send_packet(fd, "E02");
		}
		return send_packet(fd, "OK");
	}
	return send_packet(fd, "");
}

int gdb_rsp_run(uint16 port)
{
	int server_fd, client_fd, reuse = 1, close_client = FALSE;
	struct sockaddr_in address;
	char packet[RSP_PACKET_SIZE + 1];
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) return -1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	/* A debugger socket has full memory and execution control; keep it local. */
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons(port);
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(server_fd, 1) < 0) { close(server_fd); return -1; }
	printf("gdb server listening on port %u\n", port);
	fflush(stdout);
	for (;;) {
		client_fd = accept(server_fd, NULL, NULL);
		if (client_fd < 0) continue;
		/* Breakpoints belong to the previous GDB session. */
		cpuctrl_del_all_break(BREAK_POINT_TYPE_FOREVER);
		cputhr_control_dbg_waitfor_cpu_stopped();
		while (!close_client) {
			int len = read_packet(client_fd, packet, sizeof(packet));
			if (len == 0) break;
			if (len < 0) continue;
			int result = dispatch_packet(client_fd, packet, &close_client);
			if (result > 0) {
				close(client_fd);
				close(server_fd);
				return 1;
			}
			if (result < 0) break;
		}
		close(client_fd);
		close_client = FALSE;
	}
}
#else
int gdb_rsp_run(uint16 port) { (void)port; return -1; }
#endif
