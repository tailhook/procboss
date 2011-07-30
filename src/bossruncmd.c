#include "bossruncmd.h"
#include "procman.h"

#ifdef NOACTIONS

#define ACTION(a) NULL

#else

#define ACTION(a) (a)
extern void stop_supervisor();

#endif


command_def_t bossrun_cmd_table[] = {
   {name: "restart", type: CMD_PROCMAN, fun:{noarg: ACTION(restart_processes)},
    description: "Restart specified processes"},
   {name: "stop", type: CMD_PROCMAN, fun: {noarg: ACTION(stop_processes)},
    description: "Stop specified processes"},
   {name: "start", type: CMD_PROCMAN, fun: {noarg: ACTION(start_processes)},
    description: "Start specified processes if not already running"},
   {name: "shutdown", type: CMD_NOARG, fun: {noarg: ACTION(stop_supervisor)},
    description: "Stop all processes and quit supervisor"},
   {name: NULL}
};

