#define _POSIX_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>

#include "config.h"
#include "runcommand.h"
#include "linenoise.h"

config_main_t config;
int signal_fd;
int stopping = FALSE;
int nostdin = FALSE;

void init_signals() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGWINCH);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    signal_fd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
    if(signal_fd < 0) {
        perror("Can't create signalfd");
        abort();
    }
}

void send_all(int sig) {
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        status_t s = item->value._entry.status;
        if(s == PROC_STARTING || s == PROC_ALIVE
            || s == PROC_STOPPING) {
            kill(item->value._entry.pid, sig);
        }
    }
}

void decide_dead(config_process_t *process) {
    if(stopping) return;
    if(config.bossrun.failfast && !process->_entry.pending_restart) {
        stopping = TRUE;
        send_all(SIGTERM);
        return;
    }
    if(config.bossrun.restart || process->_entry.pending_restart) {
        fork_and_run(process);
    }
}

void restart_process(config_process_t *process) {
    status_t s = process->_entry.status;
    if(s == PROC_STARTING || s == PROC_ALIVE) {
        process->_entry.pending_restart = TRUE;
        process->_entry.status = PROC_STOPPING;
        kill(process->_entry.pid, SIGTERM);
    } else if(s != PROC_STOPPING) {
        fork_and_run(process);
    }
}

void reap_children() {
    while(1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if(pid == 0) break;
        if(pid == -1) {
            if(errno != EINTR && errno != ECHILD) {
                perror("Can't wait for child");
                abort();
            }
            break;
        }
        CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
            status_t s = item->value._entry.status;
            if((s == PROC_STARTING || s == PROC_ALIVE
                || s == PROC_STOPPING) && item->value._entry.pid == pid) {
                item->value._entry.status = PROC_DEAD;
                live_processes -= 1;

                decide_dead(&item->value);

                break;
            }
        }
    }
}

void reap_signal() {
    while(1) {
        struct signalfd_siginfo info;
        int bytes = read(signal_fd, &info, sizeof(info));
        if(!bytes) return;
        if(bytes < 0) {
            if(errno == EAGAIN || errno == EINTR) return;
            perror("Error reading from eventfd");
            abort();
        }
        switch(info.ssi_signo) {
        case SIGCHLD:
            reap_children();
            break;
        case SIGPIPE:
            // Nothing currently
            break;
        case SIGINT:
            if(config.bossrun.stop_signals.sigint) {
                stopping = TRUE;
            }
            send_all(info.ssi_signo);
            break;
        case SIGQUIT:
            if(config.bossrun.stop_signals.sigquit) {
                stopping = TRUE;
            }
            send_all(info.ssi_signo);
            break;
        case SIGTERM:
            if(config.bossrun.stop_signals.sigterm) {
                stopping = TRUE;
            }
            send_all(info.ssi_signo);
            break;
        case SIGHUP:
            if(config.bossrun.stop_signals.sighup) {
                stopping = TRUE;
            }
            send_all(info.ssi_signo);
            break;
        default:
            send_all(info.ssi_signo);
            break;
        }
    }
}

void read_stdin() {
    char data[1024];
    int res = read(0, data, sizeof(data));
    if(res == 0) return;
    if(res < 0) {
        if(res < 0) {
            if(errno == EINTR || errno == EAGAIN)
                return;
            perror("Can't read from stdin");
            abort();
        }
    }
    if(res > strlen("restart ")
        && !strncmp(data, "restart ", strlen("restart "))) {
        char *procname = data + strlen("restart ");
        int proclen = res - strlen("restart ");
        while(isspace(procname[--proclen]));
        CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
            if(!strncmp(item->key, procname, proclen)) {
                restart_process(&item->value);
            }
        }
    }
}

int main(int argc, char **argv) {
    config_load(&config, argc, argv);
    init_signals();
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        fork_and_run(&item->value);
    }
    while(live_processes > 0) {
        struct pollfd socks[2] = {
            { fd: signal_fd,
              events: POLLIN },
            { fd: 0,
              events: POLLIN },
            };
        int res = poll(socks, nostdin ? 1 : 2, -1);
        if(res < 0) {
            if(errno == EINTR || errno == EAGAIN) {
                continue;
            }
            perror("Can't poll");
            abort();
        }
        if(res) {
            if(socks[0].revents & POLLIN) {
                reap_signal();
            }
            if(socks[1].revents & POLLIN) {
                read_stdin();
            }
        }
    }
    config_free(&config);
    return 0;
}
