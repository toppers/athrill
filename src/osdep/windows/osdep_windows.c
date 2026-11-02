#include "osdep.h"
#include "cpu.h"
#include "std_device_ops.h"
#include "cpuemu_ops.h"

void osdep_install_terminate_handler(void (*cleanup)(void))
{
	(void)cleanup;
}

void osdep_handle_terminate_request(void)
{
}

void osdep_elaps_reset(struct DeviceClockType *clock)
{
	(void)clock;
}

void osdep_elaps_start(struct DeviceClockType *clock)
{
	(void)clock;
}

void osdep_elaps_end(struct DeviceClockType *clock)
{
	(void)clock;
}

void osdep_elaps_copy(const struct DeviceClockType *clock, struct CpuEmuElapsType *elaps)
{
	(void)clock;
	(void)elaps;
}

void osdep_sync_skipped_clock(uint64 min_intr_interval, uint32 cpu_freq,
	uint32 sync_time, uint32 show_skip_time)
{
	(void)min_intr_interval;
	(void)cpu_freq;
	(void)sync_time;
	(void)show_skip_time;
}

std_bool osdep_supports_mapped_memory(void)
{
	return FALSE;
}

Std_ReturnType osdep_open_mapped_file(const char *path, OsdepMappedFile *mapped_file)
{
	(void)path;
	(void)mapped_file;
	return STD_E_INVALID;
}

Std_ReturnType osdep_map_open_file(OsdepMappedFile *mapped_file, uint32 length)
{
	(void)mapped_file;
	(void)length;
	return STD_E_INVALID;
}
