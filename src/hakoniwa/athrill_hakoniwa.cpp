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
static uint64 world_limit_clock; /* world_time_usec * MHz read last */
static uint64 last_notified_usec = (uint64)-1;
static bool ready;  /* checks passed since the last START */
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
	uint64 delta_usec, uint64 max_delay_usec, uint64 sleep_msec,
	std_bool disable_conductor, int (*on_reset)(void))
{
	asset_name = name;
	world_time_usec = 0;
	running = registered = owns_conductor = finalized = false;
	real_sleep_msec = sleep_msec;
	world_limit_clock = 0;
	last_notified_usec = (uint64)-1;
	reset_callback = on_reset;
	pdudef_path = pdudef_path != NULL ? pdudef_path : ATHRILL_HAKONIWA_DEFAULT_PDUDEF;
	if (disable_conductor == FALSE) {
		if (hakoniwa_master_init() != 0) {
			std::cerr << "hakoniwa: hakoniwa_master_init failed\n";
			return 1;
		}
		hakoniwa_master_set_config_simtime(max_delay_usec, delta_usec);
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
	/* The stage polls the request: finalize uses the Hakoniwa API it owns. */
	osdep_terminate_from_main_loop();
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
		ready = false;
		hakoniwa_asset_notify_write_pdu_done(asset_name);
		(void)hakoniwa_asset_start_feedback_ok(asset_name);
		break;
	case HAKO_EVENT_STOP:
		running = false;
		ready = false;
		(void)hakoniwa_asset_stop_feedback_ok(asset_name);
		break;
	case HAKO_EVENT_RESET:
		running = false;
		ready = false;
		/* Report the reset only once it has been done, and report failure. */
		if (reset_callback == NULL || reset_callback() != 0) {
			std::cerr << "hakoniwa: athrill reset failed\n";
			(void)hakoniwa_asset_reset_feedback(asset_name, false);
			athrill_hakoniwa_finalize();
			exit(1);
		}
		world_time_usec = 0;
		(void)hakoniwa_asset_reset_feedback_ok(asset_name);
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

/*
 * Time synchronisation design (kept here because it is easy to forget)
 *
 * 1. The rule an asset must keep.
 *    core-pro's hako_asset_impl_execute() advances an asset from
 *    current_usec to current_usec + delta_usec only when that does not
 *    exceed the Hakoniwa (world) time:
 *        if (current_usec + delta_usec > world_time) return false;
 *    Until then the asset may run freely. Athrill applies the same rule
 *    with a step of one clock: the CPU may execute while
 *        clock < world_time_usec * cpu_mhz
 *    and must never run ahead of the world time. (The old athrill-device
 *    hakotime only stopped clock skip; a program that never halted kept
 *    executing past the world time. That is what this gate fixes.)
 *
 * 2. No asset step (delta) of its own.
 *    The world time only moves in conductor steps (--delta-usec), so a
 *    separate asset step would only make Athrill synchronise more coarsely
 *    without adding anything. Athrill's devices count clocks, so clocks
 *    are the natural unit here.
 *
 * 3. Report in usec, and only when caught up.
 *    The asset time is reported as clock / cpu_mhz (usec, truncated).
 *    It is reported only when the CPU reaches the world time read last,
 *    together with running the owned conductor, polling events and
 *    reading the world time again; while it then waits for the world
 *    time to advance, the same value is not reported again (only
 *    get_worldtime() and the event poll repeat). That is enough because
 *    the conductor only needs the asset time to decide whether the world
 *    time may advance, and Athrill needs the world time to advance only
 *    when it has caught up. Reporting more often would cost shared-memory access
 *    (with locks) without letting anything progress sooner.
 *
 * 4. Cost.
 *    Touching shared memory on every clock (the first version) made a
 *    run about 7x slower than a plain run. Between catch-ups the fast
 *    path below only compares two integers.
 *
 * 5. Consequences.
 *    - STOP/RESET are noticed at the next catch-up, i.e. at most one
 *      world-time lead of simulated time later.
 *    - Clock skip (halted CPU) is capped at the world-time limit.
 *    - --real-sleep-msec N sleeps N ms of wall-clock time after every
 *      N ms of simulated time (N * 1000 * cpu_mhz clocks); default none.
 */
extern "C" std_bool athrill_hakoniwa_stage(uint64 clock, uint32 cpu_mhz,
	uint64 *min_intr_interval)
{
	if (asset_name == NULL || finalized) return TRUE;
	osdep_handle_terminate_request();
	if (running) {
		pace_real_time(clock, cpu_mhz);
	}
	/*
	 * The CPU runs freely, clock by clock, while it stays below the world
	 * time read last (converted to clocks). Shared memory is touched only
	 * when it reaches that limit.
	 */
	if (running && clock < world_limit_clock) {
		cap_skip_interval(world_limit_clock - clock, min_intr_interval);
		return TRUE;
	}
	/*
	 * Same rule as core-pro's hako_asset_impl_execute(), with a step of one
	 * clock: report the asset time in usec, and advance only while it does
	 * not exceed the Hakoniwa (world) time.
	 */
	for (;;) {
		/*
		 * Report first, then run the owned conductor, so the world time can
		 * advance in the same pass. Report only when the value changed:
		 * while waiting, repeating the same time would only take
		 * shared-memory locks away from the conductor and other assets.
		 */
		if (running && cpu_mhz != 0 && clock / cpu_mhz != last_notified_usec) {
			last_notified_usec = clock / cpu_mhz;
			hakoniwa_asset_notify_simtime(asset_name, last_notified_usec);
		}
		if (owns_conductor) {
			(void)hakoniwa_master_execute();
		}
		/* SIGTERM and events must be polled while waiting too. */
		osdep_handle_terminate_request();
		if (handle_event()) {
			clock = 0;
			world_limit_clock = 0;
			next_sleep_clock = 0;
			last_notified_usec = (uint64)-1;
		}
		/*
		 * Simulation mode, PDU creation and PDU sync mode only change
		 * around START/STOP/RESET, so they are checked until they pass once
		 * after START (ready) and again only after the next event.
		 */
		if (running && cpu_mhz != 0 && !ready) {
			if (hakoniwa_asset_is_simulation_mode() != 0
				&& hakoniwa_asset_is_pdu_created() != 0) {
				if (hakoniwa_asset_is_pdu_sync_mode(asset_name) != 0) {
					hakoniwa_asset_notify_write_pdu_done(asset_name);
				}
				else {
					ready = true;
				}
			}
		}
		if (running && cpu_mhz != 0 && ready) {
			{
				/* The only way to learn that the world time advanced. */
				world_time_usec = hakoniwa_asset_get_worldtime();
				const uint64 limit = world_time_usec * (uint64)cpu_mhz;
				if (clock < limit) {
					world_limit_clock = limit;
					cap_skip_interval(limit - clock, min_intr_interval);
					return TRUE;
				}
			}
		}
		std::this_thread::yield();
	}
}

