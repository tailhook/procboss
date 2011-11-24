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


void send_all(int sig) {
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(&item->value._entries.running) {
            kill_single(&item->value, sig);
        }
    }
}

void restart_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        process_entry_t *entry;
        CIRCLEQ_FOREACH(entry, &processes[i]->_entries.entries, cq) {
            entry->dead = DEAD_RESTART;
            kill(entry->pid, SIGTERM);
        }
    }
}

void stop_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        processes[i]->_entries.want_down = TRUE;
        process_entry_t *entry;
        CIRCLEQ_FOREACH(entry, &processes[i]->_entries.entries, cq) {
            entry->dead = DEAD_STOP;
            kill(entry->pid, SIGTERM);
        }
    }
}

void start_processes(int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        while(processes[i]->_entries.running < processes[i]->min_instances) {
            fork_and_run(processes[i]);
        }
    }
}

void startin_proc(char *term, int nproc, config_process_t *processes[]) {
    for(int i = 0; i < nproc; ++i) {
        if(processes[i]->_entries.running < processes[i]->max_instances) {
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
