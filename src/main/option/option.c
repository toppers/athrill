#include "option/option.h"
#include "option/argument_parser.h"
#include "file.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static CmdOptionType cmd_option;

static int cmd_atoi(char *arg, uint64 *out)
{
	char *endptr;
	long long ret64;
	/*
	 * 10進数チェック
	 */
	errno = 0;
	ret64 = strtoll((const char*)arg, &endptr, 10);
	if ((errno == 0) && (*endptr == '\0')) {
		*out = ret64;
		return 0;
	}
	return -1;
}

static int cmd_atoi32(char *arg, uint32 *out)
{
	char *endptr;
	uint32 ret32;
	/*
	 * 10進数チェック
	 */
	errno = 0;
	ret32 = strtol((const char*)arg, &endptr, 10);
	if ((errno == 0) && (*endptr == '\0')) {
		*out = ret32;
		return 0;
	}
	return -1;
}

CmdOptionType *parse_args(int argc, const char* argv[])
{
	  int opt;
	  ArgumentParserState parser;
	  static const ArgumentParserLongOption long_options[] = {
		  { "hakoniwa", 1, 256 },
		  { "pdudef", 1, 257 },
		  { "delta-usec", 1, 258 },
		  { "max-delay-usec", 1, 259 },
		  { "disable-conductor", 0, 260 },
		  { "real-sleep-msec", 1, 261 },
		  { NULL, 0, 0 }
	  };

	  cmd_option.fifocfgpath = NULL;
	  cmd_option.load_filepath = NULL;
	  cmd_option.is_binary_data = FALSE;
	  cmd_option.is_interaction = FALSE;
	  cmd_option.is_remote = FALSE;
	  cmd_option.timeout = 0;
	  cmd_option.hakoniwa_asset_name = NULL;
	  cmd_option.hakoniwa_pdudef_path = NULL;
	  cmd_option.hakoniwa_delta_usec = 1000;
	  cmd_option.hakoniwa_max_delay_usec = 20000;
	  cmd_option.hakoniwa_disable_conductor = FALSE;
	  cmd_option.hakoniwa_real_sleep_msec = 0;
	  argument_parser_init(&parser);

	  while ((opt = argument_parser_next_long(&parser, argc, argv, "irbt:p:d:c:m:", long_options)) != -1) {
		  switch (opt) {
		  case 'i':
	    	cmd_option.is_interaction = TRUE;
	        break;
		  case 'r':
	    	cmd_option.is_remote = TRUE;
	        break;
	      case 'b':
		    cmd_option.is_binary_data = TRUE;
	        break;
	      case 't':
					if (cmd_atoi((char *)parser.argument, &cmd_option.timeout) < 0) {
							printf("error! -t %s\n", parser.argument);
						return NULL;
					}
	        break;
	      case 'c':
					if (cmd_atoi32((char *)parser.argument, &cmd_option.core_id_num) < 0) {
							printf("error! -c %s\n", parser.argument);
						return NULL;
					}
	        break;
	      case 'p':
			memcpy(cmd_option.buffer_fifopath, parser.argument, strlen(parser.argument));
			cmd_option.buffer_fifopath[strlen(parser.argument)] = '\0';
	        cmd_option.fifocfgpath = cmd_option.buffer_fifopath;
	        break;
	      case 'm':
			memcpy(cmd_option.buffer_memfile, parser.argument, strlen(parser.argument));
			cmd_option.buffer_memfile[strlen(parser.argument)] = '\0';
	        cmd_option.memfilepath = cmd_option.buffer_memfile;
	        break;
	      case 'd':
			memcpy(cmd_option.buffer_devcfgpath, parser.argument, strlen(parser.argument));
			cmd_option.buffer_devcfgpath[strlen(parser.argument)] = '\0';
	        cmd_option.devcfgpath = cmd_option.buffer_devcfgpath;
	        break;
		  case 256:
			cmd_option.hakoniwa_asset_name = (char *)parser.argument;
			break;
		  case 257:
			cmd_option.hakoniwa_pdudef_path = (char *)parser.argument;
			break;
		  case 258:
			if (cmd_atoi((char *)parser.argument, &cmd_option.hakoniwa_delta_usec) < 0 || cmd_option.hakoniwa_delta_usec == 0) {
				printf("error! --delta-usec %s\n", parser.argument);
				return NULL;
			}
			break;
		  case 259:
			if (cmd_atoi((char *)parser.argument, &cmd_option.hakoniwa_max_delay_usec) < 0 || cmd_option.hakoniwa_max_delay_usec == 0) {
				printf("error! --max-delay-usec %s\n", parser.argument);
				return NULL;
			}
			break;
		  case 260:
			cmd_option.hakoniwa_disable_conductor = TRUE;
			break;
		  case 261:
			if (cmd_atoi((char *)parser.argument, &cmd_option.hakoniwa_real_sleep_msec) < 0) {
				printf("error! --real-sleep-msec %s\n", parser.argument);
				return NULL;
			}
			break;
	      default:
	        printf("parse_args:error! \'%c\' \'%c\'\n", opt, parser.error_option);
	        return NULL;
	    }
	  }
#if 0
	  printf("i = %d\n", cmd_option.is_interaction);
	  printf("b = %d\n", cmd_option.is_binary_data);
	  printf("t = %llu\n", cmd_option.timeout);
	  printf("p = %s\n", (cmd_option.fifocfgpath != NULL) ? cmd_option.fifocfgpath : "NULL");
#endif
		if (parser.index >= argc) {
			printf("ERROR: not found <load file>\n");
			return NULL;
		}
	  memcpy(cmd_option.load_file.filepath.str, argv[parser.index], strlen(argv[parser.index]));
	  cmd_option.load_file.filepath.str[strlen(argv[parser.index])] = '\0';
	  cmd_option.load_file.filepath.len = strlen(argv[parser.index]);
      cmd_option.load_filepath = (char *)cmd_option.load_file.filepath.str;

      if (file_load(&cmd_option.load_file) == FALSE) {
    	  return NULL;
      }

      if ((cmd_option.fifocfgpath != NULL) &&(file_exist(cmd_option.fifocfgpath) == FALSE)) {
    	  printf("ERROR: not found fifo(%s)\n", cmd_option.fifocfgpath);
    	  return NULL;
      }
      if ((cmd_option.devcfgpath != NULL) &&(file_exist(cmd_option.devcfgpath) == FALSE)) {
    	  printf("ERROR: not found devcfg(%s)\n", cmd_option.devcfgpath);
    	  return NULL;
      }
      if ((cmd_option.memfilepath != NULL) &&(file_exist(cmd_option.memfilepath) == FALSE)) {
    	  printf("ERROR: not found memory.txt(%s)\n", cmd_option.memfilepath);
    	  return NULL;
      }

	  return &cmd_option;
}
