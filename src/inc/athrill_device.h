#ifndef _ATHRILL_DEVICE_H_
#define _ATHRILL_DEVICE_H_

#include "std_types.h"
#include "cpu.h"
#include "std_device_ops.h"
#include "cpu.h"

extern void device_init_athrill_device(void);
extern void device_init_athrill_exdev(void);
extern void device_add_athrill_exdev(void (*supply_clock) (DeviceClockType *));

extern void device_supply_clock_athrill_device(void);
extern void device_supply_clock_exdev(DeviceClockType *dev_clock);


typedef struct {
	int fd;
	void *addr;
} AthrillDeviceMmapInfoType;
extern void athrill_device_set_mmap_info(AthrillDeviceMmapInfoType *info);

extern void athrill_syscall_device(uint32 addr);

#endif /* _ATHRILL_DEVICE_H_ */
