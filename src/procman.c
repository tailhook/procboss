#define _POSIX_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>
#include <termios.h>

#include "procman.h"
#include "globalstate.h"
#include "runcommand.h"
#include "log.h"

static inline int CHECK(int res, char *msg) {
    if(res < 0) {
        logstd(LOG_STARTUP, msg);
        exit(127);
    }
    return res;
}

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

void startin_proc(char *term, int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(processes[i]->_entry.pending != PENDING_RESTART) {
            processes[i]->_entry.pending = PENDING_UP;
        }
        if(!PROCESS_EXISTS(processes[i])) {
            int parentpid = getpid();
            int pid = do_fork(processes[i]);
            if(!pid) {
                int fd = open(term, O_RDWR);
                CHECK(fd, "Can't open terminal");
                if(fd != 0) CHECK(dup2(fd, 0), "Can't dup to 0");
                if(fd != 1) CHECK(dup2(fd, 1), "Can't dup to 1");
                if(fd != 2) CHECK(dup2(fd, 2), "Can't dup to 2");
                CHECK(setsid(), "Can't become session leader");
                if(ioctl(fd, TIOCSCTTY, 0) < 0)
                    logstd(LOG_STARTUP, "Can't attach terminal");
                if(fd != 0 && fd != 1 && fd != 2) close(fd);
                do_run(processes[i], parentpid);
            }
        }
    }
}

void signal_proc(char *sig, int nproc, config_process_t *processes[]) {
    int signum = atoi(sig);
    for(int i = 0; i < nproc; ++i) {
        if(PROCESS_EXISTS(processes[i])) {
            kill(processes[i]->_entry.pid, signum);
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
