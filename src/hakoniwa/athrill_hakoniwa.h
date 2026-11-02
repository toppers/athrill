#ifndef ATHRILL_HAKONIWA_H
#define ATHRILL_HAKONIWA_H

#include "std_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int athrill_hakoniwa_initialize(const char *asset_name, const char *pdudef_path,
	uint64 delta_usec, uint64 conductor_delta_usec, uint64 max_delay_usec, uint64 real_sleep_msec,
	std_bool disable_conductor, int (*reset_callback)(void));
std_bool athrill_hakoniwa_is_enabled(void);
std_bool athrill_hakoniwa_stage(uint64 clock, uint32 cpu_mhz,
	uint64 *min_intr_interval);
void athrill_hakoniwa_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
