#ifndef _OPTION_H_
#define _OPTION_H_

#include "std_types.h"
#include "file.h"

typedef struct {
	std_bool	is_interaction;
	uint32	core_id_num;
	uint64	timeout;
	std_bool	is_binary_data;
	std_bool	is_remote;

	char	*fifocfgpath;
	char	buffer_fifopath[4096];

	char	*load_filepath;
	FileType load_file;

	char	*devcfgpath;
	char	buffer_devcfgpath[4096];

	char*				memfilepath;
	char				buffer_memfile[4096];

	char	*hakoniwa_asset_name;
	char	*hakoniwa_pdudef_path;
	uint64	hakoniwa_delta_usec;
	uint64	hakoniwa_max_delay_usec;
	uint64	hakoniwa_real_sleep_msec;
	std_bool hakoniwa_disable_conductor;
} CmdOptionType;

extern CmdOptionType *parse_args(int argc, const char* argv[]);


#endif /* _OPTION_H_ */
