#include "osdep.h"
#include "cpu.h"
#include "std_device_ops.h"
#include "cpuemu_ops.h"
#include "target/target_os_api.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static void (*osdep_cleanup)(void);

static void osdep_sigterm_handler(int arg)
{
	(void)arg;
	if (osdep_cleanup != NULL) {
		osdep_cleanup();
	}
	sync();
	sync();
	sync();
	sync();
	sync();
	exit(0);
}

void osdep_install_terminate_handler(void (*cleanup)(void))
{
	osdep_cleanup = cleanup;
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, osdep_sigterm_handler);
}

void osdep_elaps_reset(struct DeviceClockType *clock)
{
	memset(&clock->start_tv, 0, sizeof(struct timeval));
	memset(&clock->elaps_tv, 0, sizeof(struct timeval));
}

void osdep_elaps_start(struct DeviceClockType *clock)
{
	(void)gettimeofday(&clock->start_tv, NULL);
}

void osdep_elaps_end(struct DeviceClockType *clock)
{
	struct timeval current;
	struct timeval elaps_tv;

	if (clock->start_tv.tv_sec == 0) {
		return;
	}
	(void)gettimeofday(&current, NULL);
	cpuemu_timeval_sub(&current, &clock->start_tv, &elaps_tv);
	cpuemu_timeval_add(&elaps_tv, &clock->elaps_tv, &clock->elaps_tv);
}

void osdep_elaps_copy(const struct DeviceClockType *clock, struct CpuEmuElapsType *elaps)
{
	elaps->elaps_tv = clock->elaps_tv;
}

void osdep_sync_skipped_clock(uint64 min_intr_interval, uint32 cpu_freq,
	uint32 sync_time, uint32 show_skip_time)
{
	uint64 skipc_usec = 0;
	if (sync_time > 0) {
		skipc_usec = ((min_intr_interval - 1) / cpu_freq);
		if (skipc_usec > (uint64)sync_time) {
			(void)usleep((useconds_t)(skipc_usec - (uint64)sync_time));
		}
	}
	if (show_skip_time != 0) {
		static struct timeval prev_elaps;
		struct timeval result;
		struct timeval elaps;
		(void)gettimeofday(&elaps, NULL);
		cpuemu_timeval_sub(&elaps, &prev_elaps, &result);
		printf("skip-clock = "PRINT_FMT_UINT64" : %ld sec %ld usec sync_time=%u \n",
			skipc_usec, (long)result.tv_sec, (long)result.tv_usec, (unsigned int)sync_time);
		prev_elaps = elaps;
	}
}

std_bool osdep_supports_mapped_memory(void)
{
	return TRUE;
}

Std_ReturnType osdep_open_mapped_file(const char *path, OsdepMappedFile *mapped_file)
{
	struct stat statbuf;

	mapped_file->fd = open(path, O_RDWR);
	mapped_file->error_number = errno;
	if (mapped_file->fd < 0) {
		return STD_E_NOENT;
	}
	if (fstat(mapped_file->fd, &statbuf) < 0) {
		mapped_file->error_number = errno;
		return STD_E_INVALID;
	}
	mapped_file->file_size = (uint64)statbuf.st_size;
	mapped_file->address = NULL;
	return STD_E_OK;
}

Std_ReturnType osdep_map_open_file(OsdepMappedFile *mapped_file, uint32 length)
{
	mapped_file->address = mmap(NULL, length, PROT_READ | PROT_WRITE,
		MAP_SHARED, mapped_file->fd, 0);
	if (mapped_file->address == MAP_FAILED) {
		mapped_file->error_number = errno;
		return STD_E_INVALID;
	}
	return STD_E_OK;
}
