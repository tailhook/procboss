#define _POSIX_SOURCE
#include <sys/types.h>
#include <signal.h>

#include "procman.h"
#include "globalstate.h"
#include "runcommand.h"

void restart_process(config_process_t *process) {
    status_t s = process->_entry.status;
    if(s == PROC_STARTING || s == PROC_ALIVE) {
        process->_entry.pending = PENDING_RESTART;
        process->_entry.status = PROC_STOPPING;
        kill(process->_entry.pid, SIGTERM);
    } else if(s != PROC_STOPPING) {
        fork_and_run(process);
    }
}

void send_all(int sig) {
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(PROCESS_EXISTS(&item->value)) {
            kill(item->value._entry.pid, sig);
        }
    }
}

void restart_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        restart_process(processes[i]);
    }
}

void stop_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        processes[i]->_entry.pending = PENDING_DOWN;
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGTERM);
        }
    }
}

void start_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(processes[i]->_entry.pending != PENDING_RESTART) {
            processes[i]->_entry.pending = PENDING_UP;
        }
        if(!PROCESS_EXISTS(processes[i])) {
            fork_and_run(processes[i]);
        }
    }
}

void sigterm_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGTERM);
        }
    }
}

void sighup_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGHUP);
        }
    }
}

void sigkill_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGKILL);
        }
    }
}

void sigusr1_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGUSR1);
        }
    }
}

void sigusr2_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGUSR2);
        }
    }
}

void sigint_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGINT);
        }
    }
}

void sigquit_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, SIGQUIT);
        }
    }
}
