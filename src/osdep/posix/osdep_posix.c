#include "osdep.h"
#include "cpu.h"
#include "std_device_ops.h"
#include "cpuemu_ops.h"
#include "target/target_os_api.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * SIGTERM is blocked in every thread and received by a dedicated thread
 * with sigwait(), so the cleanup never runs inside a signal handler (it is
 * not async-signal-safe). By default that thread runs the cleanup and
 * exits. After osdep_terminate_from_main_loop() it only records the
 * request, and the main loop runs the cleanup from
 * osdep_handle_terminate_request() (Hakoniwa: the main loop owns the
 * Hakoniwa API, which the cleanup uses too).
 */
static pthread_mutex_t osdep_terminate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t osdep_terminate_once = PTHREAD_ONCE_INIT;
static void (*osdep_cleanup)(void);
static std_bool osdep_terminate_polled = FALSE;
/* Written by the SIGTERM thread, read by the main loop: an atomic, not volatile. */
static atomic_int osdep_terminate_requested = 0;

static void osdep_run_cleanup_and_exit(void)
{
	void (*cleanup)(void);

	pthread_mutex_lock(&osdep_terminate_mutex);
	cleanup = osdep_cleanup;
	pthread_mutex_unlock(&osdep_terminate_mutex);
	if (cleanup != NULL) {
		cleanup();
	}
	sync();
	exit(0);
}

static void *osdep_terminate_thread(void *arg)
{
	sigset_t signals;
	int received;
	std_bool polled;

	(void)arg;
	sigemptyset(&signals);
	sigaddset(&signals, SIGTERM);
	for (;;) {
		if ((sigwait(&signals, &received) == 0) && (received == SIGTERM)) {
			break;
		}
	}
	pthread_mutex_lock(&osdep_terminate_mutex);
	polled = osdep_terminate_polled;
	pthread_mutex_unlock(&osdep_terminate_mutex);
	atomic_store_explicit(&osdep_terminate_requested, 1, memory_order_release);
	if (!polled) {
		osdep_run_cleanup_and_exit();
	}
	return NULL;
}

/* Called before any other thread exists, so they all inherit the mask. */
static void osdep_start_terminate_thread(void)
{
	sigset_t signals;
	pthread_t thread;

	sigemptyset(&signals);
	sigaddset(&signals, SIGTERM);
	(void)pthread_sigmask(SIG_BLOCK, &signals, NULL);
	if (pthread_create(&thread, NULL, osdep_terminate_thread, NULL) == 0) {
		(void)pthread_detach(thread);
	}
}

void osdep_install_terminate_handler(void (*cleanup)(void))
{
	pthread_mutex_lock(&osdep_terminate_mutex);
	osdep_cleanup = cleanup;
	pthread_mutex_unlock(&osdep_terminate_mutex);
	signal(SIGPIPE, SIG_IGN);
	(void)pthread_once(&osdep_terminate_once, osdep_start_terminate_thread);
}

void osdep_terminate_from_main_loop(void)
{
	pthread_mutex_lock(&osdep_terminate_mutex);
	osdep_terminate_polled = TRUE;
	pthread_mutex_unlock(&osdep_terminate_mutex);
}

void osdep_handle_terminate_request(void)
{
	if (atomic_load_explicit(&osdep_terminate_requested, memory_order_acquire) == 0) {
		return;
	}
	osdep_run_cleanup_and_exit();
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
