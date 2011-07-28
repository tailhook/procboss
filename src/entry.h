#ifndef _H_ENTRY
#define _H_ENTRY

#include <sys/types.h>

typedef enum {
    PROC_NEW,
    PROC_STARTING,
    PROC_ALIVE,
    PROC_STOPPING,
    PROC_DEAD,
    PROC_ERROR
} status_t;

typedef struct process_entry_s {
    status_t status;
    pid_t pid;
} process_entry_t;

extern int live_processes;

#endif
