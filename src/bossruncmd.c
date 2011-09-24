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
   {name: "sig", type: CMD_PROCMAN1, fun: {noarg: ACTION(signal_proc)},
    description: "Send signal by number"},
   {name: "sigterm", type: CMD_PROCMAN, fun: {noarg: ACTION(sigterm_processes)},
    description: "Send TERM to specified processes"},
   {name: "sighup", type: CMD_PROCMAN, fun: {noarg: ACTION(sighup_processes)},
    description: "Send HUP to specified processes"},
   {name: "sigkill", type: CMD_PROCMAN, fun: {noarg: ACTION(sigkill_processes)},
    description: "Send KILL to specified processes"},
   {name: "sigusr1", type: CMD_PROCMAN, fun: {noarg: ACTION(sigusr1_processes)},
    description: "Send USR1 to specified processes"},
   {name: "sigusr2", type: CMD_PROCMAN, fun: {noarg: ACTION(sigusr2_processes)},
    description: "Send USR2 to specified processes"},
   {name: "sigint", type: CMD_PROCMAN, fun: {noarg: ACTION(sigint_processes)},
    description: "Send INT to specified processes"},
   {name: "sigquit", type: CMD_PROCMAN, fun: {noarg: ACTION(sigquit_processes)},
    description: "Send QUIT to specified processes"},
   {name: "shutdown", type: CMD_NOARG, fun: {noarg: ACTION(stop_supervisor)},
    description: "Stop all processes and quit supervisor"},
   {name: NULL}
};

