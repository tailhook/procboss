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
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

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
    int tpsid;
    char *cmd;
    long long uptime;
    long threads;
    long vsize;
    long rss;
    long cpu;
} process_info_t;

void print_usage(FILE *file) {
    fprintf(file,
        "Usage:\n"
        "   bosstree [ -acdmpurtT ] [-A | -C config.yaml | -P pid]\n"
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
        "   -m       Monitor (continuously display all the processes)\n"
        "\n"
        );
}

int parse_options(int argc, char **argv, bosstree_opt_t *options) {
    int opt;
    while((opt = getopt(argc, argv, "AC:P:dcahptmNruU")) != -1) {
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
    char filename[64];
    sprintf(filename, "/proc/%d/stat", pid);
    int fd = open(filename, O_RDONLY);
    if(!fd) return FALSE;
    char buf[4096];
    int apid;
    int len = read(fd, buf, 4095);
    assert(len >= 0);
    buf[len] = 0;
    long utime, stime, cutime, cstime;
    sscanf(buf,
        "%d %*s %*c %d %d %d %d %d %*u %*u %*u %*u %*u"
        " %lu %lu %lu %lu %*d %ld %*d %llu %lu %ld",
        &apid, &info->ppid, &info->pgid, &info->psid,
        &info->tty, &info->tpsid,
        &utime, &stime, &cutime, &cstime,
        &info->threads, &info->uptime,
        &info->vsize, &info->rss);
    assert(apid == pid);
    close(fd);
    return TRUE;
}

int parse_arguments(int pid, process_info_t *info) {
    char filename[64];
    sprintf(filename, "/proc/%d/cmdline", pid);
    int fd = open(filename, O_RDONLY);
    if(!fd) return FALSE;
    char buf[4096];
    int len = read(fd, buf, 4096);
    assert(len >= 0);
    info->cmd = malloc(len);
    memcpy(info->cmd, buf, len);
    info->cmd[len-1] = 0;
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
    for(int i = 0; i < num; ++i) {
        if(tbl[i].kind == BOSS) {
            int bosspid = tbl[i].pid;
            printf("%d %s\n", tbl[i].pid, tbl[i].cmd);
            for(int j = 0; j < num; ++j) {
                if(tbl[j].bosspid == bosspid && tbl[j].kind == BOSS_CHILD) {
                    printf("    %d %s\n", tbl[j].pid, tbl[j].name);
                }
            }
        }
    }
    if(options->detached) {
        for(int i = 0; i < num; ++i) {
            if(tbl[i].kind == DETACHED) {
                printf("%d %s\n", tbl[i].pid, tbl[i].name);
            }
        }
    }
}

void sort_processes(process_info_t *tbl, int num) {
    for(int i = 0; i < num; ++i) {
        if(tbl[i].bosspid) {
            int bosspid = tbl[i].bosspid;
            int ok = FALSE;
            for(int j = 0; j < num; ++j) {
                if(tbl[j].pid == bosspid) {
                    ok = TRUE;
                    if(tbl[j].kind == BOSS) break;  // Already visited
                    tbl[j].kind = BOSS;
                    for(int k = 0; k < num; ++k) {
                        if(tbl[k].bosspid == bosspid) {
                            if(tbl[k].pid == tbl[k].mypid) {
                                tbl[k].kind = BOSS_CHILD;
                            } else {
                                tbl[k].kind = BOSS_ANCESTOR;
                            }
                        }
                    }
                    break;
                }
            }
            if(!ok) {
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
        show_uptime: FALSE,
        show_name: FALSE,
        show_rss: FALSE,
        show_cpu: FALSE,
        monitor: FALSE
        };
    parse_options(argc, argv, &options);
    process_info_t *tbl;
    int num = find_processes(&tbl);
    sort_processes(tbl, num);
    print_processes(tbl, num, &options);

    return 0;
}
