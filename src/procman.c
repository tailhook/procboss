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

void kill_single(config_process_t *process, int signal) {
    process_entry_t *entry;
    CIRCLEQ_FOREACH(entry, &process->_entries.entries, cq) {
        kill(entry->pid, signal);
    }
}


void restart_process(process_entry_t *process) {
    process->all->pending = PENDING_RESTART;
    kill(process->pid, SIGTERM);
}

void send_all(int sig) {
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(&item->value._entries.running) {
            kill_single(&item->value, sig);
        }
    }
}

void restart_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        restart_process(processes[i]);
    }
}

void stop_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        processes[i]->all->pending = PENDING_DOWN;
        kill(processes[i]->pid, SIGTERM);
    }
}

void start_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(processes[i]->all->pending != PENDING_RESTART) {
            processes[i]->all->pending = PENDING_UP;
        }
        if(processes[i]->all->running < processes[i]->config->min_instances) {
            fork_and_run(processes[i]->config);
        }
    }
}

void startin_proc(char *term, int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(processes[i]->all->pending != PENDING_RESTART) {
            processes[i]->all->pending = PENDING_UP;
        }
        if(processes[i]->all->running < processes[i]->config->max_instances) {
            int parentpid = getpid();
            int pid = do_fork(processes[i]->config);
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
                do_run(processes[i]->config, parentpid);
            }
        }
    }
}

void signal_proc(char *sig, int nproc, process_entry_t *processes[]) {
    int signum = atoi(sig);
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, signum);
    }
}

void sigterm_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, SIGTERM);
    }
}

void sighup_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, SIGHUP);
    }
}

void sigkill_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, SIGKILL);
    }
}

void sigusr1_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, SIGUSR1);
    }
}

void sigusr2_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, SIGUSR2);
    }
}

void sigint_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, SIGINT);
    }
}

void sigquit_processes(int nproc, process_entry_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        kill(processes[i]->pid, SIGQUIT);
    }
}
