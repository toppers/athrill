#include "cpu_config.h"
#include "mpu.h"
#include "mpu_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"
#include "mpu_malloc.h"
#ifdef EXDEV_ENABLE
#include "athrill_device.h"
#include "athrill_exdev.h"
#include "shared_library.h"
#endif

static Std_ReturnType memory_get_data8(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint8 *data);
static Std_ReturnType memory_get_data16(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint16 *data);
static Std_ReturnType memory_get_data32(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint32 *data);
static Std_ReturnType memory_put_data8(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint8 data);
static Std_ReturnType memory_put_data16(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint16 data);
static Std_ReturnType memory_put_data32(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint32 data);
static Std_ReturnType memory_get_pointer(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint8 **data);

MpuAddressRegionOperationType	default_memory_operation = {
		.get_data8 		= 	memory_get_data8,
		.get_data16		=	memory_get_data16,
		.get_data32		=	memory_get_data32,

		.put_data8 		= 	memory_put_data8,
		.put_data16		=	memory_put_data16,
		.put_data32		=	memory_put_data32,

		.get_pointer	= memory_get_pointer
};

#if 0
static inline bool has_permission(MpuAddressRegionType *region, CoreIdType core_id)
{
	if (	(region->permission != MPU_ADDRESS_REGION_PERM_ALL) &&
			((region->permission & (1U << core_id)) == FALSE) ) {
		return FALSE;
	}
	return TRUE;
}
#endif

/*
 * The containment test used by the region searches: the whole access
 * [addr, addr + search_size) must lie inside the region.
 */
static inline std_bool region_covers(const MpuAddressRegionType *region, AthrillAddrType addr, AthrillAddrType search_size)
{
	AthrillAddrType start = region->start;
	AthrillAddrType end = start + region->size;
	AthrillAddrType paddr_str = (addr & region->mask);
	AthrillAddrType paddr_end = paddr_str + search_size;

	return ((start <= paddr_str) && (paddr_str < end)) &&
		((start < paddr_end) && (paddr_end <= end));
}

#ifdef SEARCH_REGION_OPTIMIZE
/*
 * Each search remembers the index of the ROM/RAM region it found last and
 * tries it first, with the same containment test as the full search. An
 * index, not a pointer: dynamic_map is reallocated when regions are added.
 * (It replaces a 1 KiB page cache that started at page 0 with index -1 and
 * did not check the access size against the region end.)
 */
static inline MpuAddressRegionType *previous_region_covers(int previous, AthrillAddrType addr, AthrillAddrType search_size)
{
	MpuAddressRegionType *region;

	if ((previous < 0) || ((uint32)previous >= mpu_address_map.dynamic_map_num)) {
		return NULL;
	}
	region = &mpu_address_map.dynamic_map[previous];
	if ((region->data == NULL) || !region_covers(region, addr, search_size)) {
		return NULL;
	}
	return region;
}
#endif

// オリジナルのsearch_region
// 最適化の際はこちらをwrite用、search_region_rをreadように使用することで同じページ内へのアクセスを
// 発生しやすくする。ここを治す場合はsearch_region_rも同じように直すこと
static inline MpuAddressRegionType *search_region(CoreIdType core_id, AthrillAddrType addr, AthrillAddrType search_size)
{
	uint32 i;

	//printf("addr=0x%x\n", addr);

#ifdef SEARCH_REGION_OPTIMIZE
	static int previous_index = -1;
	MpuAddressRegionType *previous_region = previous_region_covers(previous_index, addr, search_size);

	if (previous_region != NULL) {
		return previous_region;
	}
#endif


	for (i = 0U; i < mpu_address_map.dynamic_map_num; i++) {
		AthrillAddrType start = mpu_address_map.dynamic_map[i].start;
		AthrillAddrType end = start + mpu_address_map.dynamic_map[i].size;
		AthrillAddrType paddr_str = (addr & mpu_address_map.dynamic_map[i].mask);
		AthrillAddrType paddr_end = paddr_str + search_size;

		if (	((start <= paddr_str) && (paddr_str < end)) &&
				((start <  paddr_end) && (paddr_end <= end))
			) {

#if 0
			//printf("1:passed1\n");
			if (has_permission( &mpu_address_map.dynamic_map[i], core_id) == FALSE) {
				printf("search_region:permission error:addr=0x%x\n", addr);
				return NULL;
			}
#endif
			if (mpu_address_map.dynamic_map[i].data == NULL) {
				//MALLOC not malloced region
				return NULL;
			}
			//printf("2:passed1:%u:0x%p\n", i,  &mpu_address_map.map[i]);
#ifdef SEARCH_REGION_OPTIMIZE
			previous_index = (int)i;
#endif
			return &mpu_address_map.dynamic_map[i];
		}
	}


	for (i = 0U; i < MPU_CONFIG_REGION_NUM; i++) {
		AthrillAddrType start = mpu_address_map.map[i].start;
		AthrillAddrType end = start + mpu_address_map.map[i].size;
		AthrillAddrType paddr_str = (addr & mpu_address_map.map[i].mask);
		AthrillAddrType paddr_end = paddr_str + search_size;

#if 0
		printf("%u:start=0x%x end=0x%x mask=0x%x\n", i, start, end, mpu_address_map.map[i].mask);
		printf("%u:pstart=0x%x pend=0x%x\n", i, paddr_str, paddr_end);
#endif
		if (	((start <= paddr_str) && (paddr_str < end)) &&
				((start <  paddr_end) && (paddr_end <= end))
			) {
#if 0
			//printf("1:passed1\n");
			if (has_permission( &mpu_address_map.map[i], core_id) == FALSE) {
				printf("search_region:permission error:addr=0x%x\n", addr);
				return NULL;
			}
#endif
			//printf("2:passed1:%u:0x%p\n", i,  &mpu_address_map.map[i]);
			return &mpu_address_map.map[i];
		}
	}
	printf("%s():search_region:not found error:addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
	return NULL;
}

// Read region
// 
#ifdef SEARCH_REGION_OPTIMIZE
static inline MpuAddressRegionType *search_region_r(CoreIdType core_id, AthrillAddrType addr, AthrillAddrType search_size)
{
	uint32 i;

	//printf("addr=0x%x\n", addr);

#ifdef SEARCH_REGION_OPTIMIZE
	static int previous_index = -1;
	MpuAddressRegionType *previous_region = previous_region_covers(previous_index, addr, search_size);

	if (previous_region != NULL) {
		return previous_region;
	}
#endif


	for (i = 0U; i < mpu_address_map.dynamic_map_num; i++) {
		AthrillAddrType start = mpu_address_map.dynamic_map[i].start;
		AthrillAddrType end = start + mpu_address_map.dynamic_map[i].size;
		AthrillAddrType paddr_str = (addr & mpu_address_map.dynamic_map[i].mask);
		AthrillAddrType paddr_end = paddr_str + search_size;

		if (	((start <= paddr_str) && (paddr_str < end)) &&
				((start <  paddr_end) && (paddr_end <= end))
			) {

#if 0
			//printf("1:passed1\n");
			if (has_permission( &mpu_address_map.dynamic_map[i], core_id) == FALSE) {
				printf("search_region:permission error:addr=0x%x\n", addr);
				return NULL;
			}
#endif
			if (mpu_address_map.dynamic_map[i].data == NULL) {
				//MALLOC not malloced region
				return NULL;
			}
			//printf("2:passed1:%u:0x%p\n", i,  &mpu_address_map.map[i]);
#ifdef SEARCH_REGION_OPTIMIZE
			previous_index = (int)i;
#endif
			return &mpu_address_map.dynamic_map[i];
		}
	}


	for (i = 0U; i < MPU_CONFIG_REGION_NUM; i++) {
		AthrillAddrType start = mpu_address_map.map[i].start;
		AthrillAddrType end = start + mpu_address_map.map[i].size;
		AthrillAddrType paddr_str = (addr & mpu_address_map.map[i].mask);
		AthrillAddrType paddr_end = paddr_str + search_size;

#if 0
		printf("%u:start=0x%x end=0x%x mask=0x%x\n", i, start, end, mpu_address_map.map[i].mask);
		printf("%u:pstart=0x%x pend=0x%x\n", i, paddr_str, paddr_end);
#endif
		if (	((start <= paddr_str) && (paddr_str < end)) &&
				((start <  paddr_end) && (paddr_end <= end))
			) {
#if 0
			//printf("1:passed1\n");
			if (has_permission( &mpu_address_map.map[i], core_id) == FALSE) {
				printf("search_region:permission error:addr=0x%x\n", addr);
				return NULL;
			}
#endif
			//printf("2:passed1:%u:0x%p\n", i,  &mpu_address_map.map[i]);
			return &mpu_address_map.map[i];
		}
	}
	printf("%s():search_region:not found error:addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
	return NULL;
}
#else
// Original 
#define search_region_r search_region
#endif

static MpuAddressRegionType *mpu_address_search_region(AthrillAddrType addr, AthrillAddrType size)
{
	uint32 i;
	MpuAddressRegionType *region = NULL;

	for (i = 0U; i < mpu_address_map.dynamic_map_num; i++) {
		AthrillAddrType start = mpu_address_map.dynamic_map[i].start;
		AthrillAddrType end = start + mpu_address_map.dynamic_map[i].size;
		AthrillAddrType paddr_str = (addr & mpu_address_map.dynamic_map[i].mask);
		AthrillAddrType paddr_end = paddr_str + size;

		if (	((start <= paddr_str) && (paddr_str < end)) &&
				((start <  paddr_end) && (paddr_end <= end))
			) {
			region = &mpu_address_map.dynamic_map[i];
			break;
		}
	}

	return region;
}

uint8 *mpu_address_get_rom(AthrillAddrType addr, AthrillAddrType size)
{
	MpuAddressRegionType *region = NULL;

	region = mpu_address_search_region(addr, size);
	if ((region != NULL) && (region->type == READONLY_MEMORY)) {
		return region->data;
	}
	return NULL;
}
uint8 *mpu_address_get_ram(AthrillAddrType addr, AthrillAddrType size)
{
	MpuAddressRegionType *region = NULL;

	region = mpu_address_search_region(addr, size);
	if ((region != NULL) && (region->type == GLOBAL_MEMORY)) {
		return region->data;
	}
	return NULL;
}

uint8 *mpu_address_set_rom_ram(MpuAddressGetType getType, AthrillAddrType addr, AthrillAddrType size, void *mmap_addr)
{
	MpuAddressRegionType *region = NULL;

	region = mpu_address_search_region(addr, size);

	if (region == NULL) {
		mpu_address_map.dynamic_map_num++;
		mpu_address_map.dynamic_map = realloc(mpu_address_map.dynamic_map, (sizeof(MpuAddressRegionType)) * mpu_address_map.dynamic_map_num);
		ASSERT(mpu_address_map.dynamic_map != NULL);
		mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].start = addr;
		mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].is_malloc = FALSE;

		mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].permission	= MPU_ADDRESS_REGION_PERM_ALL;
		mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].mask		= MPU_ADDRESS_REGION_MASK_ALL;
		mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].ops			= &default_memory_operation;

		if (getType == MpuAddressGetType_ROM) {
			mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].type = READONLY_MEMORY;
			mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].size = size;
			mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data = malloc(size);
		}
		else if (getType == MpuAddressGetType_RAM) {
			mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].type = GLOBAL_MEMORY;
			mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].size = size;
			mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data = calloc(1U, size);
		}
		else {
#ifdef OS_LINUX
			mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].size = size;
			if (getType == MpuAddressGetType_MMAP) {
				/* MMAP */
				mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].type = GLOBAL_MEMORY;
				mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data = mmap_addr;
			}
			else if (getType == MpuAddressGetType_MALLOC) {
				/* MALLOC */
				mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].type = GLOBAL_MEMORY;
				mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].is_malloc = TRUE;
				mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data = NULL;
			}
#endif /* OS_LINUX */
		}
		if (getType == MpuAddressGetType_MALLOC) {
			ASSERT(mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data == NULL);
		}
		else {
			ASSERT(mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data != NULL);
		}
		return mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data;
	}
	else {
		return region->data;
	}
}
#ifdef EXDEV_ENABLE
uint8 *mpu_address_set_dev(AthrillAddrType addr, AthrillAddrType size, void *handler)
{
	MpuAddressRegionType *region = NULL;
	char error_message[512];
	AthrillExDeviceType *exdev;

	region = mpu_address_search_region(addr, size);
	if (region != NULL) {
		printf("ERROR: addr=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT " already found existing region(soff=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT ")\n",
				addr, size, region->start, region->size);
		return NULL;
	}
	exdev = athrill_shared_library_symbol(
		handler, "athrill_ex_device", error_message, sizeof(error_message));
	if (exdev == NULL) {
		printf("ERROR: addr=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT " can not load exdev symbol: %s\n",
				addr, size, error_message);
		return NULL;
	}
	if (exdev->devinit == NULL) {
		printf("ERROR: addr=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT " not found ex_device_init\n", addr, size);
		return NULL;
	}
	if (exdev->supply_clock == NULL) {
		printf("ERROR: addr=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT " not found ex_device_supply_clock\n", addr, size);
		return NULL;
	}

	if (exdev->ops == NULL) {
		printf("INFO: addr=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT ": default memory operation\n", addr, size);
		exdev->ops = &default_memory_operation;
	}
	if (exdev->datap == NULL) {
		printf("INFO: addr=0x%" ATHRILL_ADDR_HEX_FORMAT " size=%" ATHRILL_ADDR_DEC_FORMAT ": default memory\n", addr, size);
		exdev->datap = malloc(size);
		ASSERT(exdev->datap != NULL);
		memset(exdev->datap, 0, size);
	}

	mpu_address_map.dynamic_map_num++;
	mpu_address_map.dynamic_map = realloc(mpu_address_map.dynamic_map, (sizeof(MpuAddressRegionType)) * mpu_address_map.dynamic_map_num);
	ASSERT(mpu_address_map.dynamic_map != NULL);
	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].type = DEVICE;
	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].start = addr;
	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].size = size;
	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].is_malloc = FALSE;

	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].permission	= MPU_ADDRESS_REGION_PERM_ALL;
	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].mask		= MPU_ADDRESS_REGION_MASK_ALL;
	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data		= (uint8*)exdev->datap;
	mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].ops			= exdev->ops;

	device_add_athrill_exdev(
		exdev,
		&mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1],
		handler);
	return mpu_address_map.dynamic_map[mpu_address_map.dynamic_map_num -1].data;
}
#endif

void mpu_address_set_malloc_region(AthrillAddrType addr, AthrillAddrType size)
{
	MpuAddressRegionType *region = NULL;

	region = mpu_address_search_region(addr, size);

	mpu_malloc_add_region(region);
	return;
}

MpuAddressRegionEnumType mpu_address_region_type_get(AthrillAddrType addr, std_bool *is_malloc)
{
	uint32 i;

	for (i = 0U; i < mpu_address_map.dynamic_map_num; i++) {
		AthrillAddrType start = mpu_address_map.dynamic_map[i].start;
		AthrillAddrType end = start + mpu_address_map.dynamic_map[i].size;
		AthrillAddrType paddr_str = (addr & mpu_address_map.dynamic_map[i].mask);

		if ((start <= paddr_str) && (paddr_str < end)) {
			if (is_malloc != NULL) {
				*is_malloc = mpu_address_map.dynamic_map[i].is_malloc;
			}
			return mpu_address_map.dynamic_map[i].type;
		}
	}

	for (i = 0U; i < MPU_CONFIG_REGION_NUM; i++) {
		AthrillAddrType start = mpu_address_map.map[i].start;
		AthrillAddrType end = start + mpu_address_map.map[i].size;
		AthrillAddrType paddr_str = (addr & mpu_address_map.map[i].mask);

		if ((start <= paddr_str) && (paddr_str < end)) {
			if (is_malloc != NULL) {
				*is_malloc = FALSE;
			}
			return mpu_address_map.map[i].type;
		}
	}
	//printf("%s():search_region:not found error:addr=0x%x\n", __FUNCTION__, addr);
	return REGION_UNKNOWN;
}


Std_ReturnType mpu_get_data8(CoreIdType core_id, AthrillAddrType addr, uint8 *data)
{
	MpuAddressRegionType *region = search_region_r(core_id, addr, 1U);
	if (region == NULL) {
		printf("%s():addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
		return STD_E_SEGV;
	}
	if (region->ops->get_data8 == NULL) {
		return STD_E_SEGV;
	}
	if (!CPU_HAS_PERMISSION(core_id, region->type, CpuMemoryAccess_READ, addr, 1U)) {
		return STD_E_SEGV;
	}
	AthrillAddrType paddr = (addr & region->mask);
	return region->ops->get_data8(region, core_id, paddr, data);
}
Std_ReturnType mpu_get_data16(CoreIdType core_id, AthrillAddrType addr, uint16 *data)
{
	MpuAddressRegionType *region = search_region_r(core_id, addr, 2U);
	if (region == NULL) {
		printf("%s():addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
		return STD_E_SEGV;
	}
	if (region->ops->get_data16 == NULL) {
		return STD_E_SEGV;
	}
	if (!CPU_HAS_PERMISSION(core_id, region->type, CpuMemoryAccess_READ, addr, 2U)) {
		return STD_E_SEGV;
	}
	AthrillAddrType paddr = (addr & region->mask);
	return region->ops->get_data16(region, core_id, paddr, data);
}

Std_ReturnType mpu_get_data32(CoreIdType core_id, AthrillAddrType addr, uint32 *data)
{
	MpuAddressRegionType *region = search_region_r(core_id, addr, 4U);
	if (region == NULL) {
		printf("%s():addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
		return STD_E_SEGV;
	}
	if (region->ops->get_data32 == NULL) {
		return STD_E_SEGV;
	}
	if (!CPU_HAS_PERMISSION(core_id, region->type, CpuMemoryAccess_READ, addr, 4U)) {
		return STD_E_SEGV;
	}
	AthrillAddrType paddr = (addr & region->mask);
	return region->ops->get_data32(region, core_id, paddr, data);
}


Std_ReturnType mpu_put_data8(CoreIdType core_id, AthrillAddrType addr, uint8 data)
{
	Std_ReturnType err;
	MpuAddressRegionType *region = search_region(core_id, addr, 1U);
	if (region == NULL) {
		printf("mpu_put_data8:error1:addr=0x%" ATHRILL_ADDR_HEX_FORMAT " data=%u\n", addr, data);
		return STD_E_SEGV;
	}
	if (region->ops->put_data8 == NULL) {
		printf("mpu_put_data8:error2:addr=0x%" ATHRILL_ADDR_HEX_FORMAT " data=%u\n", addr, data);
		return STD_E_SEGV;
	}
	if (region->type == READONLY_MEMORY) {
		printf("mpu_put_data8:error: can not write data on ROM :addr=0x%" ATHRILL_ADDR_HEX_FORMAT " data=%u\n", addr, data);
		return STD_E_SEGV;
	}
	if (!CPU_HAS_PERMISSION(core_id, region->type, CpuMemoryAccess_WRITE, addr, 1U)) {
		return STD_E_SEGV;
	}
	AthrillAddrType paddr = (addr & region->mask);
	err = region->ops->put_data8(region, core_id, paddr, data);
	if (err != STD_E_OK) {
		printf("mpu_put_data8:error3:addr=0x%" ATHRILL_ADDR_HEX_FORMAT " data=%u\n", addr, data);
	}
	return err;
}

Std_ReturnType mpu_put_data16(CoreIdType core_id, AthrillAddrType addr, uint16 data)
{
	MpuAddressRegionType *region = search_region(core_id, addr, 2U);
	if (region == NULL) {
		printf("%s():addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
		return STD_E_SEGV;
	}
	if (region->ops->put_data16 == NULL) {
		return STD_E_SEGV;
	}
	if (region->type == READONLY_MEMORY) {
		printf("mpu_put_data16:error: can not write data on ROM :addr=0x%" ATHRILL_ADDR_HEX_FORMAT " data=%u\n", addr, data);
		return STD_E_SEGV;
	}
	if (!CPU_HAS_PERMISSION(core_id, region->type, CpuMemoryAccess_WRITE, addr, 2U)) {
		return STD_E_SEGV;
	}
	AthrillAddrType paddr = (addr & region->mask);
	return region->ops->put_data16(region, core_id, paddr, data);
}

Std_ReturnType mpu_put_data32(CoreIdType core_id, AthrillAddrType addr, uint32 data)
{
	MpuAddressRegionType *region = search_region(core_id, addr, 4U);
	if (region == NULL) {
		printf("%s():addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
		return STD_E_SEGV;
	}
	if (region->ops->put_data32 == NULL) {
		return STD_E_SEGV;
	}
	if (region->type == READONLY_MEMORY) {
		printf("mpu_put_data32:error: can not write data on ROM :addr=0x%" ATHRILL_ADDR_HEX_FORMAT " data=%u\n", addr, data);
		return STD_E_SEGV;
	}
	if (!CPU_HAS_PERMISSION(core_id, region->type, CpuMemoryAccess_WRITE, addr, 4U)) {
		return STD_E_SEGV;
	}
	AthrillAddrType paddr = (addr & region->mask);
	return region->ops->put_data32(region, core_id, paddr, data);
}


Std_ReturnType mpu_get_pointer(CoreIdType core_id, AthrillAddrType addr, uint8 **data)
{
	MpuAddressRegionType *region = search_region(core_id, addr, 1U);
	if (region == NULL) {
		printf("%s():addr=0x%" ATHRILL_ADDR_HEX_FORMAT "\n", __FUNCTION__, addr);
		return STD_E_SEGV;
	}
	if (region->ops->get_pointer == NULL) {
		return STD_E_INVALID;
	}
	AthrillAddrType paddr = (addr & region->mask);
	return region->ops->get_pointer(region, core_id, paddr, data);
}





static Std_ReturnType memory_get_data8(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint8 *data)
{
	AthrillAddrType off = (addr - region->start);
	*data = *((uint8*)(&region->data[off]));
	return STD_E_OK;
}
static Std_ReturnType memory_get_data16(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint16 *data)
{
	AthrillAddrType off = (addr - region->start);
	*data = *((uint16*)(&region->data[off]));
	return STD_E_OK;
}
static Std_ReturnType memory_get_data32(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint32 *data)
{
	AthrillAddrType off = (addr - region->start);
	*data = *((uint32*)(&region->data[off]));
	return STD_E_OK;
}
static Std_ReturnType memory_put_data8(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint8 data)
{
	AthrillAddrType off = (addr - region->start);
	*((uint8*)(&region->data[off])) = data;
	return STD_E_OK;
}
static Std_ReturnType memory_put_data16(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint16 data)
{
	AthrillAddrType off = (addr - region->start);
	*((uint16*)(&region->data[off])) = data;
	return STD_E_OK;
}
static Std_ReturnType memory_put_data32(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint32 data)
{
	AthrillAddrType off = (addr - region->start);
	*((uint32*)(&region->data[off])) = data;
	return STD_E_OK;
}
static Std_ReturnType memory_get_pointer(MpuAddressRegionType *region, CoreIdType core_id, AthrillAddrType addr, uint8 **data)
{
	AthrillAddrType off = (addr - region->start);
	*data = ((uint8*)(&region->data[off]));
	return STD_E_OK;
}
