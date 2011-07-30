#ifndef _H_PROCMAN
#define _H_PROCMAN

#include "config.h"

void send_all(int sig);
void restart_process(config_process_t *process);
void restart_processes(int nproc, config_process_t *processes[]);
void stop_processes(int nproc, config_process_t *processes[]);
void start_processes(int nproc, config_process_t *processes[]);

#endif // _H_PROCMAN
