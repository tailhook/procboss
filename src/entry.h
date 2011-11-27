#ifndef _H_ENTRY
#define _H_ENTRY

#include <sys/types.h>
#include <sys/queue.h>
#include <time.h>


#define TIME2DOUBLE(tval) ((double)(tval).tv_sec + 0.000000001*(tval).tv_nsec)
#define TVAL2DOUBLE(tval) ((double)(tval).tv_sec + 0.000001*(tval).tv_usec)

typedef enum {
    DEAD_CRASH,
    DEAD_STOP,
    DEAD_RESTART
} dead_status_t;

typedef struct process_entry_s {
    CIRCLEQ_ENTRY(process_entry_s) cq;
    pid_t pid;
    dead_status_t dead;
    int instance_index;
    double start_time;
    struct config_process_s *config;
    struct process_entries_s *all;
} process_entry_t;

typedef struct process_entries_s {
    CIRCLEQ_HEAD(process_entries_head_s, process_entry_s) entries;
    int running;
    int want_down;
    int bad_attempts;
    double last_start_time;
    double last_dead_time;
} process_entries_t;

extern int live_processes;

#endif
