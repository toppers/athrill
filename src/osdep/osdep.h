#ifndef ATHRILL_OSDEP_H
#define ATHRILL_OSDEP_H

#include "std_types.h"
#include "std_errno.h"

/* Defined in std_device_ops.h and cpuemu_ops.h; forward-declared to keep this header free of them. */
struct DeviceClockType;
struct CpuEmuElapsType;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int fd;
	uint64 file_size;
	void *address;
	int error_number;
} OsdepMappedFile;

extern void osdep_install_terminate_handler(void (*cleanup)(void));
extern void osdep_elaps_reset(struct DeviceClockType *clock);
extern void osdep_elaps_start(struct DeviceClockType *clock);
extern void osdep_elaps_end(struct DeviceClockType *clock);
extern void osdep_elaps_copy(const struct DeviceClockType *clock, struct CpuEmuElapsType *elaps);
extern void osdep_sync_skipped_clock(uint64 min_intr_interval, uint32 cpu_freq,
	uint32 sync_time, uint32 show_skip_time);
extern std_bool osdep_supports_mapped_memory(void);
extern Std_ReturnType osdep_open_mapped_file(const char *path, OsdepMappedFile *mapped_file);
extern Std_ReturnType osdep_map_open_file(OsdepMappedFile *mapped_file, uint32 length);

#ifdef __cplusplus
}
#endif

#endif /* ATHRILL_OSDEP_H */
