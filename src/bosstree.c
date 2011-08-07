#define _BSD_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define TRUE 1
#define FALSE 0
#define COLOR(a) if(options->color) { printf("\033[%dm", (a)); }
#define COMMA if(started) { putchar(','); } else { started = TRUE; }

typedef struct bosstree_opt_s {
    int pid;
    char *config;
    int all;

    int detached;
    int children;

    int show_cmd;
    int show_pid;
    int show_uptime;
    int show_name;
    int show_threads;
    int show_rss;
    int show_cpu;

    int color;
    int monitor;
} bosstree_opt_t;

typedef enum process_kind_enum {
    UNKNOWN,
    BOSS,
    BOSS_CHILD,
    BOSS_ANCESTOR,
    DETACHED,
    DETACHED_ANCESTOR,
    SENTINEL
} process_kind_t;

typedef struct process_info_s {
    process_kind_t kind;
    int pid;

    char *bossconfig;
    int bosspid;
    char *name;
    int mypid;

    int ppid;
    int pgid;
    int psid;
    int tty;
    int tpgid;
    char *cmd;
    int cmd_len;
    long starttime;
    long threads;
    long vsize;
    long rss;
    long cpu;

    struct process_info_s *parent;
    struct process_info_s *boss;
    TAILQ_HEAD(children_s, process_info_s) children;
    TAILQ_ENTRY(child_entry_s) chentry;
} process_info_t;

enum color_enum {
    BRIGHT = 1,
    DIM = 2,
    FORE_BLACK = 30,
    FORE_RED = 31,
    FORE_GREEN = 32,
    FORE_YELLOW = 33,
    FORE_BLUE = 34,
    FORE_MAGENTA = 35,
    FORE_CYAN = 36,
    FORE_WHITE = 37,
    FORE_RESET = 39,
    BG_BLACK = 40,
    BG_RED = 41,
    BG_GREEN = 42,
    BG_YELLOW = 43,
    BG_BLUE = 44,
    BG_MAGENTA = 45,
    BG_CYAN = 46,
    BG_WHITE = 47,
    BG_RESET = 49,
};

void print_usage(FILE *file) {
    fprintf(file,
        "Usage:\n"
        "   bosstree [ -acdmpurtoUN ] [-A | -C config.yaml | -P pid]\n"
        "\n"
        "Tree selection options:\n"
        "   -A       Show all processes started from any bossd\n"
        "   -C FILE  Show only processed started from bossd with config FILE\n"
        "   -P PID   Show only processes started from bossd with pid PID\n"
        "\n"
        "Process selection options:\n"
        "   -d       Show processes which detached from bossd\n"
        "   -c       Show children of started processes\n"
        "\n"
        "Display options:\n"
        "   -a       Show command-line of each process\n"
        "   -p       Show pid of each process\n"
        "   -t       Show thread number of each process\n"
        "   -U       Don't show uptime of the process\n"
        "   -N       Don't show name of the process\n"
        "   -r       Show RSS(Resident Set Size) of each process\n"
        "   -u       Show CPU usage of each process (in monitor mode)\n"
        "   -o       Colorize output\n"
        "   -O       Dont' colorize output\n"
        "   -m       Monitor (continuously display all the processes)\n"
        "\n"
        );
}

int parse_options(int argc, char **argv, bosstree_opt_t *options) {
    int opt;
    while((opt = getopt(argc, argv, "AC:P:dcahpoOmNrtuU")) != -1) {
        switch(opt) {
        case 'A': options->all = TRUE;
            break;
        case 'C':
            options->config = realpath(optarg, NULL);
            break;
        case 'P': options->pid = atoi(optarg); break;
        case 'd': options->detached = TRUE; break;
        case 'c': options->children = TRUE; break;
        case 'a': options->show_cmd = TRUE; break;
        case 'p': options->show_pid = TRUE; break;
        case 't': options->show_threads = TRUE; break;
        case 'U': options->show_uptime = FALSE; break;
        case 'N': options->show_name = FALSE; break;
        case 'r': options->show_rss = TRUE; break;
        case 'u': options->show_cpu = TRUE; break;
        case 'm': options->monitor = TRUE; break;
        case 'o': options->color = TRUE; break;
        case 'O': options->color = FALSE; break;
        case 'h':
            print_usage(stdout);
            exit(0);
            break;
        default:
            print_usage(stderr);
            exit(1);
            break;
        }
    }
    return opt;
}

int parse_entry(int pid, char *data, int dlen, process_info_t *info) {
    char cfgpath[dlen+1];
    char pname[dlen+1];
    int bosspid;
    int childpid;
    if(sscanf(data, "%[^,],%d,%[^,],%d",
        cfgpath, &bosspid, pname, &childpid) != 4) {
        return FALSE;
    }
    info->bossconfig = strdup(cfgpath);
    info->bosspid = bosspid;
    info->name = strdup(pname);
    info->mypid = childpid;
    return TRUE;
}

int parse_stat(int pid, process_info_t *info) {
    struct stat stinfo;
    char filename[64];
    sprintf(filename, "/proc/%d/stat", pid);
    if(stat(filename, &stinfo) < 0) return FALSE;
    info->starttime = stinfo.st_ctime;

    int fd = open(filename, O_RDONLY);
    if(fd < 0) return FALSE;
    char buf[4096];
    int apid;
    int len = read(fd, buf, 4095);
    assert(len >= 0);
    buf[len] = 0;
    long utime, stime, cutime, cstime;
    sscanf(buf,
        "%d (%*[^)]) %*c %d %d %d"
        " %d %d %*u %*u %*u %*u %*u"
        " %lu %lu %ld %ld"
        " %*d %*d %ld %*d %*d %lu %ld",
        &apid, &info->ppid, &info->pgid, &info->psid,
        &info->tty, &info->tpgid,
        &utime, &stime, &cutime, &cstime,
        &info->threads, &info->vsize, &info->rss);
    info->rss *= sysconf(_SC_PAGESIZE);
    assert(apid == pid);
    close(fd);
    return TRUE;
}

int parse_arguments(int pid, process_info_t *info) {
    char filename[64];
    sprintf(filename, "/proc/%d/cmdline", pid);
    int fd = open(filename, O_RDONLY);
    if(fd < 0) return FALSE;
    char buf[4096];
    int len = read(fd, buf, 4096);
    assert(len >= 0);
    info->cmd = malloc(len);
    memcpy(info->cmd, buf, len);
    info->cmd[len-1] = 0;
    info->cmd_len = len;
    assert(info->cmd);
    close(fd);
    return TRUE;
}

int parse_environ(int pid, process_info_t *info) {
    char filename[64];
    sprintf(filename, "/proc/%d/environ", pid);
    int fd = open(filename, O_RDONLY);
    if(!fd) return FALSE;
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
            parse_entry(pid, realpos, end - realpos, info);
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
    return TRUE;
}

int find_processes(process_info_t **res) {
    int size = 64;
    int cur = 0;
    process_info_t *processes = malloc(sizeof(process_info_t)*size);
    assert(processes);

    DIR *dir = opendir("/proc");
    if(!dir) return 0;
    struct dirent *entry;
    while((entry = readdir(dir))) {
        if(cur >= size) {
            processes = realloc(processes, sizeof(process_info_t)*size*2);
            memset(processes + size, 0, size * sizeof(process_info_t));
            size *= 2;
        }
        int pid = strtol(entry->d_name, NULL, 10);
        if(!pid) continue;
        processes[cur].pid = pid;
        TAILQ_INIT(&processes[cur].children);
        if(!parse_stat(pid, &processes[cur]))
            continue;
        if(!parse_arguments(pid, &processes[cur]))
            continue;
        if(!parse_environ(pid, &processes[cur]))
            continue;
        ++cur;
    }
    closedir(dir);
    *res = processes;
    return cur;
}

void print_processes(process_info_t *tbl, int num, bosstree_opt_t *options) {
    char prefix[128];
    int prefixlen = 0;
    int started;
    int tm = time(NULL);
    for(int i = 0; i < num; ++i) {
        if(tbl[i].kind == BOSS) {
            started = FALSE;
            COLOR(FORE_GREEN);
            if(options->show_name) {
                COMMA;
                printf("%s", tbl[i].cmd);
            }
            if(options->show_pid) {
                COMMA;
                printf("%d", tbl[i].pid);
            }
            if(options->show_pid) {
                COMMA;
                printf("up %ld seconds", tm - tbl[i].starttime);
            }
            COLOR(FORE_RESET);
            printf("\n");
            strcpy(prefix, "  │");
            prefixlen = strlen(prefix);
            for(process_info_t *child = TAILQ_FIRST(&tbl[i].children);
                child; child=TAILQ_NEXT(child, chentry)) {
                if(TAILQ_NEXT(child, chentry)) {
                    printf("%.*s├─", prefixlen-3, prefix);
                } else {
                    printf("%.*s└─", prefixlen-3, prefix);
                }
                started = FALSE;
                if(options->show_name) {
                    COMMA;
                    printf("%s", child->name);
                }
                if(options->show_pid) {
                    COMMA;
                    printf("%d", child->pid);
                }
                if(options->show_uptime) {
                    COLOR(FORE_BLUE);
                    COMMA;
                    printf("up %ld seconds", tm - child->starttime);
                    COLOR(FORE_RESET);
                }
                if(options->show_threads) {
                    COLOR(FORE_BLUE);
                    COMMA;
                    printf("%ld threads", child->threads);
                    COLOR(FORE_RESET);
                }
                if(options->show_rss) {
                    COLOR(FORE_BLUE)
                    COMMA;
                    if(child->rss > 10000000000) {
                        printf("%ldGiB", child->rss >> 30);
                    } else if(child->rss > 1000000000) {
                        printf("%3.1fGiB", (double)child->rss/(1 << 30));
                    } else if(child->rss > 10000000) {
                        printf("%ldMiB", child->rss >> 20);
                    } else {
                        printf("%3.1fMiB", (double)child->rss/(1 << 20));
                    }
                    COLOR(FORE_RESET);
                }
                if(options->show_cmd) {
                    COMMA;
                    for(int i = 0; i < child->cmd_len; ++i) {
                        if(isprint(child->cmd[i])) {
                            putchar(child->cmd[i]);
                        } else if(!child->cmd[i]) {
                            putchar(' ');
                        } else {
                            printf("...");
                            break;
                        }
                    }
                }
                printf("\n");
            }
        }
    }
    if(options->detached) {
        for(int i = 0; i < num; ++i) {
            if(tbl[i].kind == DETACHED) {
                started = FALSE;
                process_info_t *child = &tbl[i];
                COLOR(FORE_RED);
                if(options->show_name) {
                    COMMA;
                    printf("%s", child->name);
                }
                if(options->show_pid) {
                    COMMA;
                    printf("%d", child->pid);
                }
                COLOR(FORE_RESET);
                printf("\n");
            }
        }
    }
}

void sort_processes(process_info_t *tbl, int num) {
    for(int i = 0; i < num; ++i) {
        int found = FALSE;
        for(int j = 0; j < num; ++j) {
            if(tbl[i].ppid == tbl[j].pid) {
                tbl[i].parent = &tbl[j];
                TAILQ_INSERT_TAIL(&tbl[j].children,
                    &tbl[i], chentry);
                if(tbl[i].bosspid == tbl[j].pid) {
                    tbl[j].kind = BOSS;
                    if(tbl[i].pid == tbl[i].mypid) {
                        tbl[i].kind = BOSS_CHILD;
                    } else {
                        tbl[i].kind = BOSS_ANCESTOR;
                    }
                    found = TRUE;
                }
                break;
            }
        }
        if(!found) {
            if(tbl[i].bosspid) {
                if(tbl[i].pid == tbl[i].mypid) {
                    tbl[i].kind = DETACHED;
                } else {
                    tbl[i].kind = DETACHED_ANCESTOR;
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    bosstree_opt_t options = {
        all: TRUE,
        config: NULL,
        pid: 0,
        detached: FALSE,
        children: FALSE,
        show_cmd: FALSE,
        show_pid: FALSE,
        show_threads: FALSE,
        show_uptime: TRUE,
        show_name: TRUE,
        show_rss: FALSE,
        show_cpu: FALSE,
        color: isatty(1),
        monitor: FALSE
        };
    parse_options(argc, argv, &options);
    process_info_t *tbl;
    int num = find_processes(&tbl);
    sort_processes(tbl, num);
    print_processes(tbl, num, &options);

    return 0;
}
