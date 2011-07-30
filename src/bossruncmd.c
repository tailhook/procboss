#include "bossruncmd.h"
#include "procman.h"

extern void stop_supervisor();

command_def_t bossrun_cmd_table[] = {
   {name: "restart", type: CMD_PROCMAN, fun: {noarg: restart_processes},
    description: "Restart specified processes"},
   {name: "stop", type: CMD_PROCMAN, fun: {noarg: stop_processes},
    description: "Stop specified processes"},
   {name: "start", type: CMD_PROCMAN, fun: {noarg: start_processes},
    description: "Start specified processes if not already running"},
   {name: "shutdown", type: CMD_NOARG, fun: {noarg: stop_supervisor},
    description: "Stop all processes and quit supervisor"},
   {name: NULL}
};

