#include "argument_parser.h"

#include <stddef.h>
#include <string.h>

void argument_parser_init(ArgumentParserState *state)
{
    state->index = 1;
    state->character_index = 0;
    state->argument = NULL;
    state->error_option = 0;
}

int argument_parser_next(
    ArgumentParserState *state,
    int argc,
    const char *const argv[],
    const char *option_string)
{
    return argument_parser_next_long(
        state, argc, argv, option_string, NULL);
}

int argument_parser_next_long(
    ArgumentParserState *state,
    int argc,
    const char *const argv[],
    const char *option_string,
    const ArgumentParserLongOption long_options[])
{
    const char *current;
    const char *definition;
    int option;

    state->argument = NULL;
    state->error_option = 0;

    if ((state->index >= argc) || (argv[state->index] == NULL)) {
        return -1;
    }

    current = argv[state->index];
    if (state->character_index == 0) {
        if ((current[0] != '-') || (current[1] == '\0')) {
            return -1;
        }
        if ((current[1] == '-') && (current[2] == '\0')) {
            state->index++;
            return -1;
        }
        if (current[1] == '-') {
            const char *name = &current[2];
            const char *equals = strchr(name, '=');
            size_t name_length = equals != NULL
                ? (size_t)(equals - name)
                : strlen(name);
            const ArgumentParserLongOption *long_option = long_options;

            while ((long_option != NULL) && (long_option->name != NULL)) {
                if ((strlen(long_option->name) == name_length)
                    && (strncmp(long_option->name, name, name_length) == 0)) {
                    break;
                }
                long_option++;
            }
            if ((long_option == NULL) || (long_option->name == NULL)) {
                state->index++;
                state->error_option = 0;
                return '?';
            }
            if (long_option->requires_argument != 0) {
                if (equals != NULL) {
                    state->argument = equals + 1;
                    state->index++;
                }
                else if ((state->index + 1) < argc) {
                    state->argument = argv[state->index + 1];
                    state->index += 2;
                }
                else {
                    state->index++;
                    state->error_option = long_option->value;
                    return '?';
                }
            }
            else {
                state->index++;
                if (equals != NULL) {
                    state->error_option = long_option->value;
                    return '?';
                }
            }
            return long_option->value;
        }
        state->character_index = 1;
    }

    option = (unsigned char)current[state->character_index++];
    definition = strchr(option_string, option);
    if ((definition == NULL) || (option == ':')) {
        state->error_option = option;
        if (current[state->character_index] == '\0') {
            state->index++;
            state->character_index = 0;
        }
        return '?';
    }

    if (definition[1] == ':') {
        if (current[state->character_index] != '\0') {
            state->argument = &current[state->character_index];
            state->index++;
        }
        else if ((state->index + 1) < argc) {
            state->argument = argv[state->index + 1];
            state->index += 2;
        }
        else {
            state->error_option = option;
            state->index++;
            state->character_index = 0;
            return '?';
        }
        state->character_index = 0;
    }
    else if (current[state->character_index] == '\0') {
        state->index++;
        state->character_index = 0;
    }

    return option;
}
