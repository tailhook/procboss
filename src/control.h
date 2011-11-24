#ifndef _H_CONTROL
#define _H_CONTROL

#include "config.h"

typedef enum command_type_enum {
    CMD_INSTMAN,  // Process instance management commands
    CMD_INSTMAN1, // Process instance management with 1 arg
    CMD_GROUPMAN, // Process group management commands
    CMD_GROUPMAN1, // Process group management commands with 1 arg
    CMD_NOARG
} command_type_t;

typedef void (*cmd_instman_t)(int nproc, process_entry_t *processes[]);
typedef void (*cmd_instman1_t)(char *arg,
    int nproc, process_entry_t *processes[]);
typedef void (*cmd_groupman_t)(int nproc, config_process_t *processes[]);
typedef void (*cmd_groupman1_t)(char *arg,
    int nproc, config_process_t *processes[]);
typedef void (*cmd_noarg_t)();

typedef struct command_def_s {
    char *name;
    char *description;
    command_type_t type;
    union {
        cmd_instman_t instman;
        cmd_instman1_t instman1;
        cmd_groupman_t groupman;
        cmd_groupman1_t groupman1;
        cmd_noarg_t noarg;
    } fun;
} command_def_t;

extern int control_fd;

void init_control(char *filename);
void read_control(command_def_t *cmdtable);
void close_control(char *filename);

#endif // _H_CONTROL
