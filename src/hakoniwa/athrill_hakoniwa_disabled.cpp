#include <iostream>
#include "hakoniwa/athrill_hakoniwa.h"

extern "C" int athrill_hakoniwa_initialize(const char *, const char *, uint64, uint64, uint64, uint64, std_bool, int (*)(void))
{
	std::cerr << "athrill2 was built without Hakoniwa support (ATHRILL_ENABLE_HAKONIWA=OFF)\n";
	return 1;
}

extern "C" std_bool athrill_hakoniwa_stage(uint64, uint32, uint64 *) { return TRUE; }
extern "C" void athrill_hakoniwa_finalize(void) {}

extern "C" std_bool athrill_hakoniwa_is_enabled(void)
{
	return FALSE;
}
