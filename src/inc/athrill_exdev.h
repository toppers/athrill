#ifndef _ATHRILL_EXDEV_H_
#define _ATHRILL_EXDEV_H_

#include "std_errno.h"
#include "mpu_types.h"
#include "std_device_ops.h"
#include "comm_buffer.h"
#include "tcp/tcp_client.h"
#include "tcp/tcp_server.h"
#include "udp/udp_comm.h"
#include "athrill_mpthread.h"
#include "serial_fifo.h"

typedef struct {
	Std_ReturnType (*get_devcfg_value) (const char* key, uint32 *value);
	Std_ReturnType (*get_devcfg_value_hex) (const char* key, uint32 *value);
	Std_ReturnType (*get_devcfg_string) (const char* key, char **value);
} AthrillExDevParamOperationType;

typedef struct {
	Std_ReturnType (*add_intr) (const char* name, uint32 intno, uint32 priority); //TODO target dependent function
	void (*raise_intr) (uint32 intno);
} AthrillExDevIntrOperationType;

typedef struct {
	/*
	 * fifo
	 */
	struct {
		Std_ReturnType (*create) (uint32 size, CommFifoBufferType *fifop);
		Std_ReturnType (*add) (CommFifoBufferType *fifop, const char* datap, uint32 datalen, uint32 *res);
		Std_ReturnType (*get) (CommFifoBufferType *fifop, char* datap, uint32 datalen, uint32 *res);
		void (*close) (CommFifoBufferType *fifop);
		void (*destroy) (CommFifoBufferType *fifop);
	} fifo;

	/*
	 * tcp
	 */
	struct {
		Std_ReturnType (*client_create) (const TcpClientConfigType *config, TcpClientType *client);
		Std_ReturnType (*client_connect) (TcpClientType *client);

		Std_ReturnType (*send) (TcpConnectionType *connection, const char *data, uint32 size, uint32 *res);
		Std_ReturnType (*receive) (TcpConnectionType *connection, char *data, uint32 size, uint32 *res);
		Std_ReturnType (*send_nblk) (TcpConnectionType *connection, const char *data, uint32 size, uint32 *res);
		Std_ReturnType (*receive_nblk) (TcpConnectionType *connection, char *data, uint32 size, uint32 *res);
		void (*client_close) (TcpConnectionType *connection);

		Std_ReturnType (*server_create) (const TcpServerConfigType *config, TcpServerType *server);
		Std_ReturnType (*server_accept) (const TcpServerType *server, TcpConnectionType *connection);
		void (*server_close) (TcpServerType *server);
	} tcp;

	/*
	 * udp
	 */
	struct {
		Std_ReturnType (*create) (const UdpCommConfigType *config, UdpCommType *comm);
		Std_ReturnType (*create_ipaddr) (const UdpCommConfigType *config, UdpCommType *comm, const char* my_ipaddr);

		Std_ReturnType (*read) (UdpCommType *comm);
		Std_ReturnType (*write) (UdpCommType *comm);
		Std_ReturnType (*remote_write) (UdpCommType *comm, const char *remote_ipaddr);
		void (*delete) (UdpCommType *comm);
	} udp;
	/*
	 * thread
	 */
	struct {
		Std_ReturnType (*init) (void);
		Std_ReturnType (*thr_register) (MpthrIdType *id, MpthrOperationType *op);
		void (*lock) (MpthrIdType id);
		void (*unlock) (MpthrIdType id);
		MpthrStatusType (*get_status) (MpthrIdType id);
		Std_ReturnType (*start_proc) (MpthrIdType id);
		Std_ReturnType (*wait_proc) (MpthrIdType id);
		Std_ReturnType (*timedwait_proc) (MpthrIdType id, sint32 timeout);
	} thread;
} AthrillExDevLibOperationType;

typedef struct {
	Std_ReturnType (*get_memory) (uint32 addr, uint8 **data);
	void (*get_serial_fifo) (uint32 channel, AthrillSerialFifoType **serial_fifop);
} AthrillExDevDeviceOperationType;

typedef struct {
	AthrillExDevParamOperationType	param;
	AthrillExDevIntrOperationType	intr;
	AthrillExDevDeviceOperationType	dev;
	AthrillExDevLibOperationType	libs;
} AthrillExDevOperationType;

extern AthrillExDevOperationType athrill_exdev_operation;

#include "athrill_exdev_header.h"
/*
 * dynamic symbol name: "athrill_ex_device"
 */
typedef struct {
	AthrillExDeviceHeaderType header;
	char *datap;
	MpuAddressRegionOperationType *ops;
	void (*devinit) (MpuAddressRegionType *, AthrillExDevOperationType *);
	void (*supply_clock) (DeviceClockType *);
	void (*cleanup) (void);
} AthrillExDeviceType;

#endif /* _ATHRILL_EXDEV_H_ */
