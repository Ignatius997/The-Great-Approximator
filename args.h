#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include <stdint.h>

#define ARGS_DEFAULT_PORT 0

#define ARGOPT_BIND_ADDRESS 'b'
#define ARGOPT_PORT         'p'
#define ARGOPT_PEER_ADDRESS 'a'
#define ARGOPT_PEER_PORT    'r'

// Structure definition for ProgramArgs (assumed to be defined elsewhere)
typedef struct {
    char *bind_address;
    uint16_t port;
    char *peer_address;
    uint16_t peer_port;
    bool _ar_provided;
} ProgramArgs;

// Functions declarations.
ProgramArgs args_default(void);

void args_validate(const ProgramArgs *program_args);
void args_load_value(char *arg, const char opt, ProgramArgs *program_args);
void args_parse(int argc, char *argv[], ProgramArgs *program_args);
void args_print(ProgramArgs *args);

#endif // ARGS_H