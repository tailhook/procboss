#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <fnmatch.h>

#include "control.h"
#include "constants.h"
#include "globalstate.h"


int control_fd = -1;
int ctl_write_fd = -1;

char fifobuf[MAX_COMMANDLINE];
int fifobuf_pos = 0;
bool ignore = FALSE;

void init_control(char *fifo) {
    int res;
    unlink(fifo);
    if(mkfifo(fifo, 0777)) {
        perror("Can't make control fifo");
        abort();
    }

    control_fd = open(fifo, O_RDONLY|O_NONBLOCK);
    if(control_fd < 0) {
        perror("Can't open fifo");
        abort();
    }
    res = fcntl(control_fd, F_SETFD, FD_CLOEXEC);
    if(res < 0) {
        perror("Can't set cloexec flag");
        abort();
    }

    // Second file descriptor is opened to leave pipe usable even after
    // actual writer closed pipe (would need to reopen pipe otherwise)
    ctl_write_fd = open(fifo, O_WRONLY|O_NONBLOCK);
    if(ctl_write_fd < 0) {
        perror("Failed to open write end of fifo");
        abort();
    }
    res = fcntl(ctl_write_fd, F_SETFD, FD_CLOEXEC);
    if(res < 0) {
        perror("Can't set cloexec flag");
        abort();
    }
}

void close_control(char *fifo) {
    close(control_fd);
    close(ctl_write_fd);
    unlink(fifo);
}

static int match(char *data, int argc, char *argv[], int pattern) {
    if(pattern) {
        for(int i = 0; i < argc; ++i) {
            if(!fnmatch(argv[i], data, 0)) {
                return TRUE;
            }
        }
    } else {
        for(int i = 0; i < argc; ++i) {
            if(!strcmp(data, argv[i])) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

void run_procman(command_def_t *cmd, int argc, char *argv[]) {
    optind = 0;
    int opt;
    int pattern = FALSE;
    int match_args = FALSE;
    int match_binary = FALSE;
    int match_name = TRUE;
    int match_pid = FALSE;
    while((opt = getopt(argc, argv, "+aAbBpPnNeE")) != -1) {
        switch(opt) {
        case 'a': match_args = TRUE; break;
        case 'A': match_args = FALSE; break;
        case 'b': match_binary = TRUE; break;
        case 'B': match_binary = FALSE; break;
        case 'p': match_pid = TRUE; break;
        case 'P': match_pid = FALSE; break;
        case 'n': match_name = TRUE; break;
        case 'N': match_name = FALSE; break;
        case 'e': pattern = TRUE; break;
        case 'E': pattern = FALSE; break;
        default:
            fprintf(stderr, "Wrong argument\n");
            fflush(stderr);
            return;
        }
    }
    config_process_t *processes[MAX_PROCESSES];
    int nproc = 0;
    CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
        if(nproc == MAX_PROCESSES) break;  // sorry
        if(match_name) {
            if(match(item->key, argc-optind, argv+optind, pattern)) {
                processes[nproc++] = &item->value;
                continue;
            }
        }
        if(match_binary) {
            char *rbind = strrchr(item->value.executable_path, '/');
            if(rbind) {
                rbind += 1;
            } else {
                rbind = item->value.executable_path;
            }
            if(match(rbind, argc-optind, argv+optind, pattern)) {
                processes[nproc++] = &item->value;
                continue;
            }
        }
        if(match_args) {
            CONFIG_STRING_LOOP(arg, item->value.arguments) {
                if(match(arg->value, argc-optind, argv+optind, pattern)) {
                    processes[nproc++] = &item->value;
                    goto CONTINUE;
                }
            }
        }
        if(match_pid && PROCESS_EXISTS(&item->value)) {
            char num[12];
            sprintf(num, "%d", item->value._entry.pid);
            num[11] = 0;
            if(match(num, argc-optind, argv+optind, pattern)) {
                processes[nproc++] = &item->value;
                continue;
            }
        }
        CONTINUE: continue;
    }
    if(nproc == MAX_PROCESSES) {
        fprintf(stderr, "Maximum processes reached\n");
        fflush(stderr);
        return;
    }
    cmd->fun.procman(nproc, processes);
}

void parse_spaces(char *data, int len, command_def_t *cmdtable) {
    char *argv[MAX_ARGS];
    int argc = 0;

    char *end = data + len;
    while(data < end) {
        while(data < end && isspace(*data)) ++data;
        char *cmd = data;
        while(data < end && !isspace(*data)) ++data;
        // TODO(tailhook) implement quotes
        *data++ = 0;  // We know there is a char '\n' even if this is *end
        argv[argc++] = cmd;
    }
    if(!argc) {
        fprintf(stderr, "Wrong command\n");
        fflush(stderr);
        return;
    }
    argv[argc] = NULL;

    for(command_def_t *def = cmdtable; def->name; ++def) {
        if(!strcmp(argv[0], def->name)) {
            switch(def->type) {
            case CMD_PROCMAN:
                run_procman(def, argc, argv);
                return;
            case CMD_NOARG:
                if(argc != 1) {
                    fprintf(stderr, "Extra arguments\n");
                    fflush(stderr);
                    return;
                }
                def->fun.noarg();
                return;
            default:
                abort();
            }
        }
    }
    fprintf(stderr, "Wrong command ``%s''\n", argv[0]);
    fflush(stderr);
    return;
}

void read_control(command_def_t *cmdtable) {
    int res = read(control_fd, fifobuf + fifobuf_pos,
                   sizeof(fifobuf) - fifobuf_pos);
    if(res < 0) {
        if(errno == EINTR || errno == EAGAIN) return;
        perror("Can't read from control socket");
        abort();
    }
    fifobuf_pos += res;
    char *end = fifobuf + fifobuf_pos;
    while(fifobuf_pos) {
        char *c = fifobuf;
        for(; c != end; ++c) {
            if(*c == '\n') {
                parse_spaces(fifobuf, c - fifobuf, cmdtable);
                if(c+1 != end) {
                    memmove(fifobuf, c+1, fifobuf_pos - (c+1 - fifobuf));
                }
                fifobuf_pos -= c+1 - fifobuf;
                break;
            }
        }
        if(c == end) break;
    }
}
