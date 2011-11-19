#ifndef _H_CONTROL
#define _H_CONTROL

#include "config.h"

typedef enum command_type_enum {
    CMD_PROCMAN,  // Process management commands
    CMD_PROCMAN1, // Process management with 1 arg
    CMD_NOARG
} command_type_t;

typedef void (*cmd_procman_t)(int nproc, process_entry_t *processes[]);
typedef void (*cmd_procman1_t)(char *terminal,
    int nproc, process_entry_t *processes[]);
typedef void (*cmd_noarg_t)();

typedef struct command_def_s {
    char *name;
    char *description;
    command_type_t type;
    union {
        cmd_procman_t procman;  // Process management commands
        cmd_procman1_t procman1;  // Process management with 1 argument
        cmd_noarg_t noarg;
    } fun;
} command_def_t;

extern int control_fd;

void init_control(char *filename);
void read_control(command_def_t *cmdtable);
void close_control(char *filename);

#endif // _H_CONTROL
