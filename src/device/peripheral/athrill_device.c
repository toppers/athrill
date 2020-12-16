#ifdef OS_LINUX

#include "athrill_device.h"
#include "mpu_ops.h"
#include "symbol_ops.h"
#include <string.h>
#include <sys/file.h>
#include "assert.h"
#include "std_device_ops.h"
#include "athrill_exdev.h"
#include "cpuemu_ops.h"


AthrillExDevOperationType athrill_exdev_operation;

static uint32 athrill_device_func_call_addr = 0x0;
static uint32 athrill_device_raise_interrupt_addr = 0x0;

typedef struct {
	bool isLocked;
	AthrillDeviceMmapInfoType info;
} AthrillDeviceMmapInfoTableEntryType;

typedef struct {
	uint32 count;
	AthrillDeviceMmapInfoTableEntryType *entry;
} AthrillDeviceMmapInfoTableType;

static AthrillDeviceMmapInfoTableType athrill_mmap_table;

void device_init_athrill_device(void)
{
    uint32 addr;
    uint32 size;
    int err;

    err = symbol_get_gl("athrill_device_func_call", 
        strlen("athrill_device_func_call"), &addr, &size);
    if (err >= 0) {
		printf("athrill_device_func_call=0x%x\n", addr);
	    athrill_device_func_call_addr = addr;
    }
    err = symbol_get_gl("athrill_device_raise_interrupt", 
        strlen("athrill_device_raise_interrupt"), &addr, &size);
    if (err >= 0) {
		printf("athrill_device_raise_interrupt=0x%x\n", addr);
	    athrill_device_raise_interrupt_addr = addr;
    }

    return;
}
static Std_ReturnType athrill_device_get_memory(uint32 addr, uint8 **data)
{
	return mpu_get_pointer(0U, addr, data);
}
typedef struct {
	AthrillExDeviceType *devp;
	MpuAddressRegionType *region;
} AthrillExtDevEntryType;
typedef struct {
	uint32 num;
	AthrillExtDevEntryType **exdevs;
} AthrillExtDevType;
static AthrillExtDevType athrill_exdev;

void device_init_athrill_exdev(void)
{
    /*
     * device operation setting
     */
    athrill_exdev_operation.param.get_devcfg_string = &cpuemu_get_devcfg_string;
    athrill_exdev_operation.param.get_devcfg_value = &cpuemu_get_devcfg_value;
    athrill_exdev_operation.param.get_devcfg_value_hex = &cpuemu_get_devcfg_value_hex;

    athrill_exdev_operation.intr.add_intr = NULL; //TODO
    athrill_exdev_operation.intr.raise_intr = &cpuemu_raise_intr;

    athrill_exdev_operation.dev.get_memory = &athrill_device_get_memory;
    athrill_exdev_operation.dev.get_serial_fifo = &athrill_device_get_serial_fifo_buffer;

    athrill_exdev_operation.libs.fifo.create = &comm_fifo_buffer_create;
    athrill_exdev_operation.libs.fifo.add = &comm_fifo_buffer_add;
    athrill_exdev_operation.libs.fifo.get = &comm_fifo_buffer_get;
    athrill_exdev_operation.libs.fifo.close = &comm_fifo_buffer_close;
    athrill_exdev_operation.libs.fifo.destroy = &comm_fifo_buffer_destroy;

    athrill_exdev_operation.libs.tcp.client_close = &tcp_connection_close;
    athrill_exdev_operation.libs.tcp.client_create = &tcp_client_create;
    athrill_exdev_operation.libs.tcp.client_connect = &tcp_client_connect;

    athrill_exdev_operation.libs.tcp.receive = &tcp_connection_receive;
    athrill_exdev_operation.libs.tcp.receive_nblk = &tcp_connection_receive_nblk;
    athrill_exdev_operation.libs.tcp.send = &tcp_connection_send;
    athrill_exdev_operation.libs.tcp.send_nblk = &tcp_connection_send_nblk;

    athrill_exdev_operation.libs.tcp.server_accept = &tcp_server_accept;
    athrill_exdev_operation.libs.tcp.server_create = &tcp_server_create;
    athrill_exdev_operation.libs.tcp.server_close = &tcp_server_close;

    athrill_exdev_operation.libs.udp.create = &udp_comm_create;
    athrill_exdev_operation.libs.udp.create_ipaddr = &udp_comm_create_ipaddr;
    athrill_exdev_operation.libs.udp.read = &udp_comm_read;
    athrill_exdev_operation.libs.udp.write = &udp_comm_write;
    athrill_exdev_operation.libs.udp.remote_write = &udp_comm_remote_write;
    athrill_exdev_operation.libs.udp.delete = &udp_server_delete;

    athrill_exdev_operation.libs.thread.init = &mpthread_init;
    athrill_exdev_operation.libs.thread.thr_register = &mpthread_register;
    athrill_exdev_operation.libs.thread.lock = &mpthread_lock;
    athrill_exdev_operation.libs.thread.unlock = &mpthread_unlock;
    athrill_exdev_operation.libs.thread.get_status = &mpthread_get_status;
    athrill_exdev_operation.libs.thread.start_proc = &mpthread_start_proc;
    athrill_exdev_operation.libs.thread.wait_proc = &mpthread_wait_proc;
    athrill_exdev_operation.libs.thread.timedwait_proc = &mpthread_timedwait_proc;

    int i;
    for (i = 0; i < athrill_exdev.num; i++) {
    	athrill_exdev.exdevs[i]->devp->devinit(athrill_exdev.exdevs[i]->region, &athrill_exdev_operation);
    }

    return;
}
void device_add_athrill_exdev(void *devp, void *region)
{
	AthrillExtDevEntryType *entryp = malloc(sizeof(AthrillExtDevEntryType));
	ASSERT(entryp != NULL);
	entryp->devp = (AthrillExDeviceType*)devp;
	entryp->region = (MpuAddressRegionType*)region;
	athrill_exdev.num++;
	athrill_exdev.exdevs = realloc(athrill_exdev.exdevs,
			 sizeof(AthrillExtDevEntryType*) * athrill_exdev.num);
	ASSERT(athrill_exdev.exdevs != NULL);
	athrill_exdev.exdevs[athrill_exdev.num - 1] = entryp;
	return;
}
void athrill_device_set_mmap_info(AthrillDeviceMmapInfoType *info)
{
	int inx = athrill_mmap_table.count;
	athrill_mmap_table.count++;
	athrill_mmap_table.entry = realloc(athrill_mmap_table.entry,
			sizeof(AthrillDeviceMmapInfoTableEntryType) * athrill_mmap_table.count);
	ASSERT(athrill_mmap_table.entry != NULL);

	athrill_mmap_table.entry[inx].isLocked = FALSE;
	athrill_mmap_table.entry[inx].info = *info;
	return;
}

static inline AthrillDeviceMmapInfoTableEntryType *getMmapInfo(void *addr)
{
	int i;
	for (i = 0; i < athrill_mmap_table.count; i++) {
		if (addr == athrill_mmap_table.entry[i].info.addr) {
			return &athrill_mmap_table.entry[i];
		}
	}
	return NULL;
}

static void do_athrill_device_func_call(void)
{
    Std_ReturnType err;
    uint32 data;

    if (athrill_device_func_call_addr == 0x0) {
        return;
    }

    err = mpu_get_data32(0U, athrill_device_func_call_addr, &data);
    if (err != 0) {
        return;
    }
    if (data == 0U) {
        return;
    }

    AthrillDeviceMmapInfoTableEntryType *mmapInfo = getMmapInfo(CAST_UINT32_TO_ADDR(data));
    if (mmapInfo == NULL) {
        athrill_syscall_device(data);
    }
    else {
    	int err;
    	if (mmapInfo->isLocked == FALSE) {
    		err = flock(mmapInfo->info.fd, LOCK_EX);
    		mmapInfo->isLocked = TRUE;
    	}
    	else {
    		err = flock(mmapInfo->info.fd, LOCK_UN);
    		mmapInfo->isLocked = FALSE;
    	}
		ASSERT(err == 0);
    }

    (void)mpu_put_data32(0U, athrill_device_func_call_addr, 0U);
	return;
}
static void do_athrill_device_external_raise_interrupt(void)
{
    Std_ReturnType err;
    uint32 data;

    if (athrill_device_raise_interrupt_addr == 0x0) {
        return;
    }

    err = mpu_get_data32(0U, athrill_device_raise_interrupt_addr, &data);
    if (err != 0) {
        return;
    }
    if (data == 0U) {
        return;
    }
    (void)mpu_put_data32(0U, athrill_device_raise_interrupt_addr, 0U);
	(void)intc_raise_intr(data);
	return;
}
#ifdef CONFIG_STAT_PERF
ProfStatType cpuemu_dev_adev1_prof;
ProfStatType cpuemu_dev_adev2_prof;

#define CPUEMU_DEV_ADEV1_PROF_START()	profstat_start(&cpuemu_dev_adev1_prof)
#define CPUEMU_DEV_ADEV1_PROF_END()		profstat_end(&cpuemu_dev_adev1_prof)
#define CPUEMU_DEV_ADEV2_PROF_START()	profstat_start(&cpuemu_dev_adev2_prof)
#define CPUEMU_DEV_ADEV2_PROF_END()		profstat_end(&cpuemu_dev_adev2_prof)
#else
#define CPUEMU_DEV_ADEV1_PROF_START()
#define CPUEMU_DEV_ADEV1_PROF_END()	
#define CPUEMU_DEV_ADEV2_PROF_START()
#define CPUEMU_DEV_ADEV2_PROF_END()	
#endif /* CONFIG_STAT_PERF */
void device_supply_clock_athrill_device(void)
{
    CPUEMU_DEV_ADEV1_PROF_START();
	do_athrill_device_func_call();
    CPUEMU_DEV_ADEV1_PROF_END();

    CPUEMU_DEV_ADEV2_PROF_START();
	do_athrill_device_external_raise_interrupt();
    CPUEMU_DEV_ADEV2_PROF_END();
    return;
}

void device_supply_clock_exdev(DeviceClockType *dev_clock)
{
    int i;
    for (i = 0; i < athrill_exdev.num; i++) {
    	athrill_exdev.exdevs[i]->devp->supply_clock(dev_clock);
    }
    return;
}

#endif /* OS_LINUX */
