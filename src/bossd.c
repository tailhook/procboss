#define _POSIX_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
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
#include "log.h"
#include "util.h"
#include "sockets.h"

config_main_t config;
int signal_fd;
int stopping = FALSE;
char *configuration_name;
int configuration_name_len;
char **recover_args;
long boottime;
int jiffie;

void restart_supervisor() {
    LWARNING("Restarting supervisor by manual command");
    execvp(recover_args[0], recover_args);
    abort(); // Can never reach here
}

void recover_signal(int realsig) {
    LCRASH("Restarting on signal %d", realsig);
    execvp(recover_args[0], recover_args);
    abort(); // Can never reach here
}

void init_signals() {
    stack_t sigstk;
    if ((sigstk.ss_sp = malloc(SIGSTKSZ)) == NULL) {
        STDASSERT2(-1, "Can't allocate memory for alternate stack");
    }
    sigstk.ss_size = SIGSTKSZ;
    sigstk.ss_flags = 0;
    STDASSERT2(sigaltstack(&sigstk,(stack_t *)0), "Can't set alternate stack");
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
    STDASSERT2(signal_fd, "Can't create signalfd");
}

void stop_supervisor() {
    stopping = TRUE;
    send_all(SIGTERM);
    return;
}

void decide_dead(process_entry_t *process, int status) {
    LDEAD("Process \"%s\":%d with pid %d dead on signal %d or with status %d",
        process->config->_name, process->instance_index, process->pid,
        WIFSIGNALED(status) ? WTERMSIG(status) : -1,
        WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    if(stopping) return;
    if(process->dead == DEAD_STOP) return;
    double delta = process->all->last_dead_time - process->start_time;
    if(process->dead == DEAD_RESTART
        || delta >= config.bossd.timeouts.successful_run) {
        process->all->bad_attempts = 0;
        fork_and_run(process->config);
        return;
    }
    process->all->bad_attempts += 1;
    // Will pick up on next loop
}

void reap_children() {
    struct timeval tm;
    gettimeofday(&tm, NULL);
    while(1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if(pid == 0) break;
        if(pid == -1) {
            if(errno != EINTR && errno != ECHILD) {
                STDASSERT2(-1, "Can't wait for a child");
            }
            break;
        }
        CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
            process_entry_t *entry;
            CIRCLEQ_FOREACH(entry, &item->value._entries.entries, cq) {
                if(entry->pid == pid) {
                    CIRCLEQ_REMOVE(&item->value._entries.entries, entry, cq);
                    item->value._entries.running -= 1;
                    item->value._entries.last_dead_time = TVAL2DOUBLE(tm);
                    live_processes -= 1;

                    decide_dead(entry, status);

                    free(entry);
                    break;
                }
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
            STDASSERT2(-1, "Error reading from signalfd");
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
            // Not sending sigint as it's probably already sent by terminal
            // send_all(info.ssi_signo);
            break;
        case SIGTERM:
            stopping = TRUE;
            send_all(info.ssi_signo);
            break;
        case SIGHUP:
            // TODO(tailhook) reload config
            break;
        }
    }
}

void write_pid(char *pidfile) {
    int fd = open(pidfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    STDASSERT2(fd, "Can't write pid file");
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
    // sorry, fixing shortcommings of coyaml
    ctx.cmdline->full_description = "";
    if(coyaml_cli_prepare(&ctx, argc, argv)) {
        if(errno == ECOYAML_CLI_HELP) {
            char *fname = strrchr(argv[0], '/');
            if(!fname) {
                fname = argv[0];
            } else {
                fname += 1;
            }
            execlp("man", "man", fname, NULL);
        }
        if(errno > ECOYAML_MAX || errno < ECOYAML_MIN) {
            perror(argv[0]);
        }
        config_free(&config);
        coyaml_context_free(&ctx);
        exit(1);
    }
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
        item->value._entries.running = 0;
        item->value._entries.want_down = 0;
        if(item->value.min_instances > item->value.max_instances) {
            item->value.max_instances = item->value.min_instances;
        }
        CIRCLEQ_INIT(&item->value._entries.entries);
    }
}

void parse_entry(int pid, char *data, int dlen) {
    char cfgpath[dlen+1];
    char pname[dlen+1];
    int bosspid;
    int childpid;
    int instance_index = -1;
    if(sscanf(data, "%[^,],%d,%[^,],%d,%d",
        cfgpath, &bosspid, pname, &childpid, &instance_index) < 4) {
        LRECOVER("Unparsable child entry for pid %d", pid);
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
            LRECOVER("Configuration path has changed for pid %d boss %d",
                pid, bosspid);
        }
    } else if(bosspid != getpid()) {
        LRECOVER("Probably another daemon with same config is running"
            " with pid %d (reported by child %d)", bosspid, pid);
        return;
    }
    int status;
    int res = waitpid(childpid, &status, WNOHANG);
    if(res != 0) {
        if(errno == ECHILD) {
            LRECOVER("Not a child marked as child %d, skipping", pid);
            return;
        } else if(res == pid) {
            LRECOVER("Process %d probably died during boss recover", pid);
            return;
        } else {
            LRECOVER("Error checking %d", pid);
            return;
        }
    }

    char filename[64];
    sprintf(filename, "/proc/%d/stat", pid);
    char buf[4096];
    int fd = open(filename, O_RDONLY);
    if(fd < 0) {
        LRECOVER("Process %d probably died during boss recover", pid);
        return;
    }
    res = read(fd, buf, sizeof(buf)-1);
    if(res < 10) {
        LRECOVER("Process %d probably died during boss recover", pid);
        return;
    }
    close(fd);
    long long startjif;
    if(sscanf(buf, "%*d (%*[^)]) %*c %*d %*d %*d"
        " %*d %*d %*u %*u %*u %*u %*u"
        " %*u %*u %*d %*d %*d %*d %*d %*d %lld",
        &startjif) != 1) {
        LRECOVER("Error parsing stat of %d", pid);
        return;
    }

    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(!strcmp(item->key, pname)) {
            LRECOVER("Process %d recovered as \"%s\":%d",
                pid, pname, instance_index);
            process_entry_t *entry = malloc(sizeof(process_entry_t));
            if(!entry) {
                LCRITICAL("No memory to recover process entries");
                abort();
            }
            entry->pid = pid;
            entry->start_time = boottime + (double)startjif/jiffie;
            entry->config = &item->value;
            entry->all = &item->value._entries;
            entry->dead = DEAD_CRASH;
            entry->instance_index = instance_index;
            process_entry_t *inspoint;
            int found = FALSE;
            CIRCLEQ_FOREACH(inspoint, &item->value._entries.entries, cq) {
                if(inspoint->instance_index > instance_index) {
                    CIRCLEQ_INSERT_BEFORE(&item->value._entries.entries,
                        inspoint, entry, cq);
                    found = TRUE;
                    break;
                }
            }
            if(!found) {
                CIRCLEQ_INSERT_TAIL(&item->value._entries.entries, entry, cq);
            }
            item->value._entries.running += 1;
            live_processes += 1;
            return;
        }
    }
    LRECOVER("Process %d named \"%s\" is still hanging while not in config",
        pid, pname);
}

void recover_processes() {
    DIR *dir = opendir("/proc");
    if(!dir) {
        LRECOVER("Can't open procfs");
        return;
    }
    struct dirent *entry;
    while((errno = 0, entry = readdir(dir))) {
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
                char *end = memchr(pos+1, 0, buf+buflen-pos-1);
                if(!end) {
                    if(pos < buf + buflen/2) {
                        LRECOVER("Too long boss child record for %d", pid);
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
    if(errno) {
        LRECOVER("Error reading procfs: %m");
    }
    closedir(dir);
}

void main_loop() {
    long last_rotation = -1;
    double rot_time = config.bossd.logging.rotation_time;
    int rot_size = config.bossd.logging.rotation_size;
    struct timeval tm;
    while(live_processes > 0 || !stopping) {
        struct pollfd socks[2] = {
            { fd: signal_fd,
              revents: 0,
              events: POLLIN },
            { fd: control_fd,
              revents: 0,
              events: POLLIN },
            };
        gettimeofday(&tm, NULL);
        double time = TVAL2DOUBLE(tm);
        if(last_rotation < 0) {
            last_rotation = (long)(time / rot_time);
        } else if(!stopping && (last_rotation != (long)(time / rot_time)
                                || logsize() > rot_size)) {
            rotatelogs();
        }
        double next_time = 0;
        if(!stopping) {
            CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
                if(item->value._entries.running
                    >= item->value.min_instances) continue;
                if(item->value._entries.want_down) continue;
                double start_time = item->value._entries.last_dead_time;
                if(item->value._entries.bad_attempts
                    > config.bossd.timeouts.retries) {
                    start_time += config.bossd.timeouts.big_restart;
                } else {
                    start_time += config.bossd.timeouts.small_restart;
                }
                if(start_time < time + 0.001) {
                    fork_and_run(&item->value);
                } else {
                    if(!next_time || start_time < next_time) {
                        next_time = start_time;
                    }
                }
            }
        }
        int sleep_ms = (int)((next_time - time)*1000);
        int res = poll(socks, 2, next_time ? sleep_ms : -1);
        if(res < 0) {
            if(errno == EINTR || errno == EAGAIN) {
                continue;
            }
            STDASSERT2(-1, "Can't poll");
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
}

void fix_environ(char **argv) {
    char buf[configuration_name_len+32];
    sprintf(buf, "BOSSD=%s,%d", configuration_name, getpid());
    char *curvalue = getenv("BOSSD");
    if(!curvalue || strcmp(curvalue, buf + 6)) {
        char *newenv[] = {buf, NULL};
        if(argv[0][0] != '/') {
            char buf2[PATH_MAX];
            STDASSERT(readlink("/proc/self/exe", buf2, PATH_MAX));
            argv[0] = buf2;
        }
        STDASSERT(execve(argv[0], argv, newenv));
    }
}

void checkdir(char *filename) {
    char dirname[strlen(filename)+1];
    strcpy(dirname, filename);
    char *lastslash = strrchr(dirname, '/');
    if(lastslash) {
        *lastslash = 0;
    }
    if(access(dirname, F_OK)) {
        mkdir(dirname, 0770);
    }
}

int main(int argc, char **argv) {
    recover_args = argv;
    read_config(argc, argv);
    fix_environ(argv);
    if(config.bossd.logging.file) {
        checkdir(config.bossd.logging.file);
    }
    openlogs();

    // This is very early because closes unneeded sockets
    // Its not very signal safe, but in most cases will inherit signal
    // mask from previous bossd which exec'd
    // At the real startup no signals are expected
    open_sockets(TRUE);

    init_signals();
    boottime = get_boot_time();
    jiffie = sysconf(_SC_CLK_TCK);
    LRECOVER("Starting");
    if(config.bossd.fifo_len) {
        checkdir(config.bossd.fifo);
        init_control(config.bossd.fifo);
    }
    if(config.bossd.pid_file_len) {
        checkdir(config.bossd.pid_file);
        write_pid(config.bossd.pid_file);
    }
    recover_processes();
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        while(item->value._entries.running < item->value.min_instances) {
            fork_and_run(&item->value);
        }
    }
    LRECOVER("Started with %d processes", live_processes);
    main_loop();
    if(config.bossd.pid_file_len) {
        unlink(config.bossd.pid_file);
    }
    if(config.bossd.fifo_len) {
        close_control(config.bossd.fifo);
    }
    config_free(&config);
    return 0;
}
