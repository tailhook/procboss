#ifndef _H_PROCMAN
#define _H_PROCMAN

#include "config.h"

void send_all(int sig);
void restart_process(process_entry_t *process);
void restart_processes(int nproc, config_process_t *processes[]);
void stop_processes(int nproc, config_process_t *processes[]);
void startin_proc(char *term, int nproc, config_process_t *processes[]);
void start_processes(int nproc, config_process_t *processes[]);
void signal_proc(char *sig, int nproc, process_entry_t *processes[]);
void sigterm_processes(int nproc, process_entry_t *processes[]);
void sighup_processes(int nproc, process_entry_t *processes[]);
void sigkill_processes(int nproc, process_entry_t *processes[]);
void sigusr1_processes(int nproc, process_entry_t *processes[]);
void sigusr2_processes(int nproc, process_entry_t *processes[]);
void sigint_processes(int nproc, process_entry_t *processes[]);
void sigquit_processes(int nproc, process_entry_t *processes[]);

#endif // _H_PROCMAN
