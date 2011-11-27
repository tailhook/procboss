#include "bossruncmd.h"
#include "procman.h"

#ifdef NOACTIONS

#define ACTION(a) NULL

#else

#define ACTION(a) (a)
extern void stop_supervisor();

#endif


command_def_t bossrun_cmd_table[] = {
   {name: "restart", type: CMD_GROUPMAN,
    fun:{groupman: ACTION(restart_processes)},
    description: "Restart specified processes"},
   {name: "stop", type: CMD_GROUPMAN,
    fun: {groupman: ACTION(stop_processes)},
    description: "Stop specified processes"},
   {name: "start", type: CMD_GROUPMAN,
    fun: {groupman: ACTION(start_processes)},
    description: "Start specified processes if not already running"},
   {name: "incr", type: CMD_GROUPMAN,
    fun: {groupman: ACTION(incr_processes)},
    description: "Add another instance of each of processes (up to max)"},
   {name: "max", type: CMD_GROUPMAN,
    fun: {groupman: ACTION(max_processes)},
    description: "Start maximum instances of a process"},
   {name: "decr", type: CMD_GROUPMAN,
    fun: {groupman: ACTION(decr_processes)},
    description: "Kill another instance of each of processes (up to max)"},
   {name: "min", type: CMD_GROUPMAN,
    fun: {groupman: ACTION(min_processes)},
    description: "Leave only minimum instances of a process"},
   {name: "norestart", type: CMD_INSTMAN,
    fun: {instman: ACTION(norestart_processes)},
    description: "Mark processes not to restart"},
   {name: "sig", type: CMD_INSTMAN1,
    fun: {instman1: ACTION(signal_proc)},
    description: "Send signal by number"},
   {name: "sigterm", type: CMD_INSTMAN,
    fun: {instman: ACTION(sigterm_processes)},
    description: "Send TERM to specified processes"},
   {name: "sighup", type: CMD_INSTMAN,
    fun:{instman: ACTION(sighup_processes)},
    description: "Send HUP to specified processes"},
   {name: "sigkill", type: CMD_INSTMAN,
    fun:{instman: ACTION(sigkill_processes)},
    description: "Send KILL to specified processes"},
   {name: "sigusr1", type: CMD_INSTMAN,
    fun: {instman: ACTION(sigusr1_processes)},
    description: "Send USR1 to specified processes"},
   {name: "sigusr2", type: CMD_INSTMAN,
    fun: {instman: ACTION(sigusr2_processes)},
    description: "Send USR2 to specified processes"},
   {name: "sigint", type: CMD_INSTMAN,
    fun: {instman: ACTION(sigint_processes)},
    description: "Send INT to specified processes"},
   {name: "sigquit", type: CMD_INSTMAN,
    fun: {instman: ACTION(sigquit_processes)},
    description: "Send QUIT to specified processes"},
   {name: "shutdown", type: CMD_NOARG,
    fun: {noarg: ACTION(stop_supervisor)},
    description: "Stop all processes and quit supervisor"},
   {name: NULL}
};

