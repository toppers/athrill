#ifndef ATHRILL_ARGUMENT_PARSER_H
#define ATHRILL_ARGUMENT_PARSER_H

typedef struct {
    int index;
    int character_index;
    const char *argument;
    int error_option;
} ArgumentParserState;

typedef struct {
    const char *name;
    int requires_argument;
    int value;
} ArgumentParserLongOption;

void argument_parser_init(ArgumentParserState *state);
int argument_parser_next(
    ArgumentParserState *state,
    int argc,
    const char *const argv[],
    const char *option_string);
int argument_parser_next_long(
    ArgumentParserState *state,
    int argc,
    const char *const argv[],
    const char *option_string,
    const ArgumentParserLongOption long_options[]);

#endif /* ATHRILL_ARGUMENT_PARSER_H */
