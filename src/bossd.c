#define _POSIX_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
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
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#include "config.h"
#include "runcommand.h"
#include "control.h"
#include "procman.h"
#include "bossdcmd.h"

config_main_t config;
int signal_fd;
int stopping = FALSE;
char *configuration_name;
int configuration_name_len;
char **recover_args;

void recover_signal(int realsig) {
    execvp(recover_args[0], recover_args);
}

void init_signals() {
    stack_t sigstk;
    if ((sigstk.ss_sp = malloc(SIGSTKSZ)) == NULL) {
        perror("Can't allocate memory for alternate stack");
        abort();
    }
    sigstk.ss_size = SIGSTKSZ;
    sigstk.ss_flags = 0;
    if (sigaltstack(&sigstk,(stack_t *)0) < 0) {
        perror("Can't set alternate stack");
        abort();
    }
    struct sigaction recover_action;
    recover_action.sa_handler = recover_signal;
    recover_action.sa_flags = SA_ONSTACK;
    sigemptyset(&recover_action.sa_mask);
    sigaction(SIGQUIT, &recover_action, NULL);
    sigaction(SIGBUS, &recover_action, NULL);
    sigaction(SIGSEGV, &recover_action, NULL);
    sigaction(SIGILL, &recover_action, NULL);
    sigaction(SIGABRT, &recover_action, NULL);
    sigaction(SIGALRM, &recover_action, NULL);
    sigaction(SIGHUP, &recover_action, NULL);  // temporarily
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    //sigaddset(&mask, SIGHUP);
    // TODO(tailhook) implement configuration reload on sighup
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGCHLD);
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

void decide_dead(char *name, config_process_t *process, int status) {
    if(stopping) return;
    if(process->_entry.pending == PENDING_UP
        || process->_entry.pending == PENDING_RESTART) {
        fork_and_run(process);
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
            status_t s = item->value._entry.status;
            if((s == PROC_STARTING || s == PROC_ALIVE
                || s == PROC_STOPPING) && item->value._entry.pid == pid) {
                item->value._entry.status = PROC_DEAD;
                live_processes -= 1;

                decide_dead(item->key, &item->value, status);

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
            stopping = TRUE;
            send_all(info.ssi_signo);
            break;
        case SIGTERM:
            stopping = TRUE;
            send_all(info.ssi_signo);
            break;
        case SIGHUP:
            // Just nothing, maybe reload config in future
            break;
        }
    }
}

void write_pid(char *pidfile) {
    int fd = open(pidfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(!fd) {
        perror("Can't write pid file");
        abort();
    }
    char buf[16];
    int nbytes = sprintf(buf, "%d", getpid());
    write(fd, buf, nbytes);
    close(fd);
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

void parse_entry(int pid, char *data, int dlen) {
    char cfgpath[dlen+1];
    char pname[dlen+1];
    int bosspid;
    int childpid;
    if(sscanf(data, "%[^,],%d,%[^,],%d",
        cfgpath, &bosspid, pname, &childpid) != 4) {
        // TODO(tailhook) report unparsable child entry
        return;
    }
    if(childpid != pid) {
        // Some child process, that's ok
        return;
    }
    if(strcmp(cfgpath, configuration_name)) {
        if(bosspid != getpid()) {
            // That's ok
        } else {
            // TODO(tailhook) report warning that config changed
        }
    } else if(bosspid != getpid()) {
        // TODO(tailhook) warn: probably another daemon running
        return;
    }
    int status;
    int res = waitpid(childpid, &status, WNOHANG);
    if(res != 0) {
        if(errno == ECHILD) {
            // TODO(tailhook) warn: apropriately marked process is not a child
            return;
        } else if(res == pid) {
            // TODO(tailhook) log as probably shutdown process
            return;
        } else {
            // TODO(tailhook) log as something unexpected
            return;
        }
    }
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(!strcmp(item->key, pname)) {
            // TODO(tailhook) log successfully recovered process
            item->value._entry.pid = pid;
            item->value._entry.status = PROC_ALIVE;
            live_processes += 1;
            return;
        }
    }
    // TODO(tailhook) print warning that there is a process which is no more in
    //                configuration file
}

void recover_processes() {
    DIR *dir = opendir("/proc");
    if(!dir) return;  // TODO(tailhook) log error
    struct dirent *entry;
    while((entry = readdir(dir))) {
        int pid = strtol(entry->d_name, NULL, 10);
        if(!pid) continue;
        char filename[64];
        sprintf(filename, "/proc/%d/environ", pid);
        int fd = open(filename, O_RDONLY);
        if(!fd) continue;
        char buf[4096] = { 0 };
        int bufof = 1;
        while(1) {
            int buflen = read(fd, buf+bufof, sizeof(buf)-bufof);
            if(buflen <= 0) break;
            buflen += bufof;
            char *pos = memmem(buf, buflen,
                               "\0BOSS_CHILD=", strlen("BOSS_CHILD=")+1);
            if(pos) {
                char *end = memchr(pos, 0, buf + buflen - pos);
                if(!end) {
                    if(pos < buf + buflen/2) {
                        // TODO(tailhook) log bad boss_child record
                        break;
                    } else {
                        memmove(buf, pos, buf + buflen - pos);
                        bufof = buf + buflen - pos;
                        continue;
                    }
                }
                char *realpos = pos + strlen("BOSS_CHILD=") + 1;
                parse_entry(pid, realpos, end - realpos);
                break;
            }
            if(buflen >= strlen("BOSS_CHILD=") + 1) {
                bufof = strlen("BOSS_CHILD=") + 1;
                memmove(buf, buf + buflen - bufof, bufof);
            } else {
                bufof = buflen;
            }
        }
        close(fd);
    }
    // TODO(tailhook) check error and log
    closedir(dir);
}

int main(int argc, char **argv) {
    recover_args = argv;
    read_config(argc, argv);
    init_signals();
    if(config.bossd.fifo_len) {
        init_control(config.bossd.fifo);
    }
    if(config.bossd.pid_file_len) {
        write_pid(config.bossd.pid_file);
    }
    recover_processes();
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(item->value._entry.status == PROC_NEW) {
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
                read_control(bossd_cmd_table);
            }
        }
    }
    if(config.bossd.pid_file_len) {
        unlink(config.bossd.pid_file);
    }
    if(config.bossd.fifo_len) {
        close_control(config.bossd.fifo);
    }
    config_free(&config);
    return 0;
}
