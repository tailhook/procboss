#ifndef _H_ENTRY
#define _H_ENTRY

#include <sys/types.h>
#include <time.h>

#define PROCESS_EXISTS(proc) ( \
    (proc)->_entry.status == PROC_STARTING \
    || (proc)->_entry.status == PROC_ALIVE \
    || (proc)->_entry.status == PROC_STOPPING)
#define TIME2DOUBLE(tval) ((double)(tval).tv_sec + 0.000000001*(tval).tv_nsec)
#define TVAL2DOUBLE(tval) ((double)(tval).tv_sec + 0.000001*(tval).tv_usec)

typedef enum {
    PROC_NEW,
    PROC_STARTING,
    PROC_ALIVE,
    PROC_STOPPING,
    PROC_DEAD,
    PROC_STOPPED,
    PROC_ERROR
} status_t;

typedef enum {
    PENDING_UP,
    PENDING_DOWN,
    PENDING_RESTART
} pending_status_t;

typedef struct process_entry_s {
    status_t status;
    pending_status_t pending;
    pid_t pid;
    // Only for bossd:
    int bad_attempts;
    double start_time;
    double dead_time;
} process_entry_t;

extern int live_processes;

#endif
