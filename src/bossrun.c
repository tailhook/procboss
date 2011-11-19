#define _BSD_SOURCE
#define _POSIX_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>
#include <unistd.h>

#include "config.h"
#include "runcommand.h"
#include "control.h"
#include "procman.h"
#include "bossruncmd.h"
#include "log.h"
#include "sockets.h"

config_main_t config;
int signal_fd;
int stopping = FALSE;
char *configuration_name = "bossrun";
int configuration_name_len = 7; // strlen("bossrun");
extern char **environ;

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

void stop_supervisor() {
    stopping = TRUE;
    send_all(SIGTERM);
    return;
}

void decide_dead(process_entry_t *process, int status) {
    if(stopping) return;
    if(config.bossrun.failfast && process->all->pending != PENDING_RESTART
                               && process->all->pending != PENDING_DOWN) {
        LWARNING(config.bossrun.failfast_message,
            process->config->_name, process->pid,
            WIFSIGNALED(status) ? WTERMSIG(status) : -1,
            WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        stop_supervisor();
        return;
    }
    if(((config.bossrun.restart && process->all->pending == PENDING_UP)
            || process->all->pending == PENDING_RESTART)
       && process->all->running < process->config->min_instances) {
        fork_and_run(process->config);
        return;
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
            process_entry_t *entry;
            CIRCLEQ_FOREACH(entry, &item->value._entries.entries, cq) {
                if(entry->pid == pid) {
                    CIRCLEQ_REMOVE(&item->value._entries.entries, entry, cq);
                    item->value._entries.running -= 1;
                    live_processes -= 1;

                    decide_dead(entry, status);

                    free(entry);
                    goto end_of_processes;
                }
            }
        }
        end_of_processes: continue;
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

void read_config(int argc, char **argv) {
    coyaml_context_t ctx;
    if(!config_context(&ctx, &config)) {
        perror(argv[0]);
        exit(1);
    }
    coyaml_cli_prepare_or_exit(&ctx, argc, argv);
    coyaml_readfile_or_exit(&ctx);
    coyaml_env_parse_or_exit(&ctx);
    coyaml_cli_parse_or_exit(&ctx, argc, argv);
    configuration_name = realpath(ctx.root_filename, NULL);
    if(!configuration_name) {
        perror(argv[0]);
        exit(1);
    }
    configuration_name_len = strlen(configuration_name);
    coyaml_context_free(&ctx);
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        item->value._name = item->key;
        item->value._name_len = item->key_len;
    }
}

void fix_environ(char **argv) {
    char buf[configuration_name_len+32];
    sprintf(buf, "BOSSRUN=%s,%d", configuration_name, getpid());
    char *curvalue = getenv("BOSSRUN");
    if(!curvalue || strcmp(curvalue, buf + 8)) {
        int envlen = 2;
        for(char **e = environ; *e; ++e)
            envlen += 1;
        char **newenv = alloca(envlen*sizeof(char*));
        memcpy(newenv, environ, (envlen-1)*sizeof(char*));
        char **e;
        for(e = newenv; *e; ++e) {
            if(!strncmp(*e, "BOSSRUN=", 8)) {
                *e = buf;
                break;
            }
        }
        if(!*e) {
            *e = buf;
            *++e = NULL;
        }
        environ = newenv;
        STDASSERT(execvp(argv[0], argv));
    }
}

int main(int argc, char **argv) {
    read_config(argc, argv);
    fix_environ(argv);
    init_signals();
    if(config.bossrun.fifo_len) {
        init_control(config.bossrun.fifo);
    }
    open_sockets(FALSE);
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        item->value._name = item->key;
        item->value._name_len = item->key_len;
        item->value._entries.running = 0;
        if(item->value.min_instances > item->value.max_instances) {
            item->value.max_instances = item->value.min_instances;
        }
        CIRCLEQ_INIT(&item->value._entries.entries);
        while(item->value._entries.running < item->value.min_instances) {
            fork_and_run(&item->value);
        }
    }
    while(live_processes > 0) {
        struct pollfd socks[2] = {
            { fd: signal_fd,
              revents: 0,
              events: POLLIN },
            { fd: control_fd,
              revents: 0,
              events: POLLIN },
            };
        int res = poll(socks, control_fd >= 0 ? 2 : 1, -1);
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
                read_control(bossrun_cmd_table);
            }
        }
    }
    config_free(&config);
    return 0;
}
