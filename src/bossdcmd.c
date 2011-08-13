#include "bossruncmd.h"
#include "procman.h"
#include "log.h"

#ifdef NOACTIONS

#define ACTION(a) NULL

#else

#define ACTION(a) (a)
extern void stop_supervisor();
extern void restart_supervisor();

#endif


command_def_t bossd_cmd_table[] = {
   {name: "restart", type: CMD_PROCMAN, fun:{noarg: ACTION(restart_processes)},
    description: "Restart specified processes"},
   {name: "stop", type: CMD_PROCMAN, fun: {noarg: ACTION(stop_processes)},
    description: "Stop specified processes"},
   {name: "start", type: CMD_PROCMAN, fun: {noarg: ACTION(start_processes)},
    description: "Start specified processes if not already running"},
   {name: "shutdown", type: CMD_NOARG, fun: {noarg: ACTION(stop_supervisor)},
    description: "Stop all processes and quit supervisor"},
   {name: "explode", type: CMD_NOARG, fun: {noarg: ACTION(restart_supervisor)},
    description: "Restart supervisor in place and check processes"},
   {name: "reopenlog", type: CMD_NOARG, fun: {noarg: ACTION(reopenlogs)},
    description: "Reopen log file in case it's rotated by external tool"},
   {name: "rotatelog", type: CMD_NOARG, fun: {noarg: ACTION(rotatelogs)},
    description: "Force log rotation"},
   {name: NULL}
};

