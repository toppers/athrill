#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#define JSON_ASSERT(x) ((void)0)
#include "hakoniwa/pdu/pdu_definition.hpp"

extern "C" {
#include "hakoniwa_asset_polling.h"
#include "cpuemu_ops.h"
#include "osdep.h"
#include "athrill_hakoniwa.h"
}

#ifndef ATHRILL_HAKONIWA_DEFAULT_PDUDEF
#define ATHRILL_HAKONIWA_DEFAULT_PDUDEF "config/hakoniwa/athrill-pdudef.json"
#endif

enum { HAKO_EVENT_NONE, HAKO_EVENT_START, HAKO_EVENT_STOP, HAKO_EVENT_RESET, HAKO_EVENT_ERROR };
static const char *asset_name;
static uint64 world_time_usec;
static uint64 asset_delta_usec;
static uint64 asset_time_usec;   /* multiple of asset_delta_usec */
static uint64 step_limit_clock;  /* (asset_time_usec + delta) * MHz once granted */
static bool running;
static bool registered;
static bool owns_conductor;
static bool finalized;
static uint64 real_sleep_msec;
static int (*reset_callback)(void);

extern "C" std_bool athrill_hakoniwa_is_enabled(void) { return TRUE; }

static void print_time(void)
{
	const uint32 cpu_mhz = cpuemu_get_cpu_frequency();
	const uint64 clock = cpuemu_get_clock();
	std::cout << "hakoniwa: athrill_clock=" << clock
		<< " asset_time_usec=" << (cpu_mhz == 0 ? 0 : clock / cpu_mhz)
		<< " reported_time_usec=" << asset_time_usec
		<< " world_time_usec=" << world_time_usec << '\n';
}

extern "C" void athrill_hakoniwa_finalize(void)
{
	if (asset_name == NULL || finalized) return;
	finalized = true;
	print_time();
	if (registered) {
		(void)hakoniwa_asset_unregister(asset_name);
		registered = false;
	}
	/* The polling master API has no release/stop call. */
	owns_conductor = false;
}

static int create_pdu_channels(const char *pdudef_path)
{
	hakoniwa::pdu::PduDefinition definition;
	if (!definition.load(pdudef_path)) return -1;
	for (const auto& entry : definition.list_entries()) {
		if (hakoniwa_asset_create_pdu_lchannel(entry.robot_name.c_str(),
			entry.def.channel_id, entry.def.pdu_size) != 0) {
			std::cerr << "hakoniwa: failed to create PDU channel "
				<< entry.robot_name << ':' << entry.def.channel_id << '\n';
			return -1;
		}
	}
	return 0;
}

extern "C" int athrill_hakoniwa_initialize(const char *name, const char *pdudef_path,
	uint64 delta_usec, uint64 conductor_delta_usec, uint64 max_delay_usec, uint64 sleep_msec,
	std_bool disable_conductor, int (*on_reset)(void))
{
	asset_name = name;
	world_time_usec = 0;
	running = registered = owns_conductor = finalized = false;
	real_sleep_msec = sleep_msec;
	asset_delta_usec = delta_usec;
	asset_time_usec = 0;
	step_limit_clock = 0;
	reset_callback = on_reset;
	pdudef_path = pdudef_path != NULL ? pdudef_path : ATHRILL_HAKONIWA_DEFAULT_PDUDEF;
	if (disable_conductor == FALSE) {
		if (hakoniwa_master_init() != 0) {
			std::cerr << "hakoniwa: hakoniwa_master_init failed\n";
			return 1;
		}
		hakoniwa_master_set_config_simtime(max_delay_usec, conductor_delta_usec);
		owns_conductor = true;
	}
	if (hakoniwa_asset_init() != 0) {
		std::cerr << "hakoniwa: hakoniwa_asset_init failed\n";
		return 1;
	}
	if (hakoniwa_asset_register_polling(name) != 0) {
		std::cerr << "hakoniwa: hakoniwa_asset_register_polling failed\n";
		return 1;
	}
	registered = true;
	if (create_pdu_channels(pdudef_path) != 0) {
		athrill_hakoniwa_finalize();
		return 1;
	}
	osdep_install_terminate_handler(athrill_hakoniwa_finalize);
	return 0;
}

static bool handle_event(void)
{
	switch (hakoniwa_asset_get_event(asset_name)) {
	case HAKO_EVENT_NONE: break;
	case HAKO_EVENT_START:
		/* Attach/load PDU storage before feedback. A valid zero-channel PDU
		 * definition has a zero-sized data area, for which this query may be 0. */
		(void)hakoniwa_asset_is_pdu_created();
		world_time_usec = hakoniwa_asset_get_worldtime();
		running = true;
		hakoniwa_asset_notify_write_pdu_done(asset_name);
		(void)hakoniwa_asset_start_feedback_ok(asset_name);
		break;
	case HAKO_EVENT_STOP:
		running = false;
		(void)hakoniwa_asset_stop_feedback_ok(asset_name);
		break;
	case HAKO_EVENT_RESET:
		running = false;
		(void)hakoniwa_asset_reset_feedback_ok(asset_name);
		if (reset_callback == NULL || reset_callback() != 0) {
			std::cerr << "hakoniwa: athrill reset failed\n";
			athrill_hakoniwa_finalize();
			exit(1);
		}
		world_time_usec = 0;
		return true;
	case HAKO_EVENT_ERROR:
		std::cerr << "hakoniwa: polling event error\n";
		athrill_hakoniwa_finalize();
		exit(1);
	default: break;
	}
	return false;
}

/*
 * Optional real-time pacing: after every real_sleep_msec of simulated time
 * (converted to Athrill clocks with the CPU frequency), sleep the same
 * amount of wall-clock time.
 */
static uint64 next_sleep_clock;

static void pace_real_time(uint64 clock, uint32 cpu_mhz)
{
	if (real_sleep_msec == 0 || cpu_mhz == 0) {
		return;
	}
	const uint64 period = real_sleep_msec * 1000ULL * (uint64)cpu_mhz;
	if (next_sleep_clock == 0) {
		next_sleep_clock = period;
	}
	if (clock >= next_sleep_clock) {
		std::this_thread::sleep_for(std::chrono::milliseconds(real_sleep_msec));
		next_sleep_clock += period;
	}
}


/* Clock skip must not jump past the world time. */
static void cap_skip_interval(uint64 interval, uint64 *min_intr_interval)
{
	if (min_intr_interval != NULL && *min_intr_interval != (uint64)-1
		&& interval < *min_intr_interval) {
		*min_intr_interval = interval;
	}
}

extern "C" std_bool athrill_hakoniwa_stage(uint64 clock, uint32 cpu_mhz,
	uint64 *min_intr_interval)
{
	if (asset_name == NULL || finalized) return TRUE;
	if (running) {
		pace_real_time(clock, cpu_mhz);
	}
	/*
	 * Inside a granted step [asset_time, asset_time + delta] the CPU runs
	 * freely and shared memory is not touched.
	 */
	if (running && clock < step_limit_clock) {
		cap_skip_interval(step_limit_clock - clock, min_intr_interval);
		return TRUE;
	}
	if (step_limit_clock != 0 && clock >= step_limit_clock) {
		asset_time_usec += asset_delta_usec;
		step_limit_clock = 0;
	}
	/*
	 * Same rule as core-pro's hako_asset_impl_execute(): report the asset
	 * time, and take the next step only while asset_time + delta does not
	 * exceed the Hakoniwa (world) time.
	 */
	for (;;) {
		if (owns_conductor) {
			(void)hakoniwa_master_execute();
		}
		if (handle_event()) {
			asset_time_usec = 0;
			step_limit_clock = 0;
			next_sleep_clock = 0;
		}
		if (running && cpu_mhz != 0) {
			hakoniwa_asset_notify_simtime(asset_name, asset_time_usec);
			if (hakoniwa_asset_is_simulation_mode() != 0
				&& hakoniwa_asset_is_pdu_created() != 0) {
				if (hakoniwa_asset_is_pdu_sync_mode(asset_name) != 0) {
					hakoniwa_asset_notify_write_pdu_done(asset_name);
				}
				else {
					world_time_usec = hakoniwa_asset_get_worldtime();
					const uint64 next_usec = asset_time_usec + asset_delta_usec;
					if (next_usec <= world_time_usec) {
						step_limit_clock = next_usec * (uint64)cpu_mhz;
						cap_skip_interval(step_limit_clock - clock, min_intr_interval);
						return TRUE;
					}
				}
			}
		}
		std::this_thread::yield();
	}
}

