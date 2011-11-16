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
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#include "util.h"
#include "config.h"

#define TRUE 1
#define FALSE 0

typedef struct bosstree_opt_s {
    int pid;
    char *config;
    int all;

    int detached;
    int children;

    int show_hier;
    int show_cmd;
    int show_pid;
    int show_uptime;
    int show_name;
    int show_threads;
    int show_rss;
    int show_vsize;
    int show_cpu;

    int color;
    double monitor;

    long boottime;
    int jiffie;
    int pagesize;
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

struct process_info_s;
struct entry_s;

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
    long long starttime;
    long threads;
    long vsize;
    long rss;
    double cpu;
    long cpu_time;
    long up_time;

    struct process_info_s *parent;
    struct process_info_s *boss;
    TAILQ_HEAD(children_s, process_info_s) children;
    TAILQ_HEAD(entries_s, entry_s) entries;
    TAILQ_ENTRY(process_info_s) chentry;
    TAILQ_ENTRY(process_info_s) runentry;
} process_info_t;

typedef enum status_enum {
    S_NORMAL,
    S_DOWN,
    S_STALLED,
    S_ORPHAN
} entrystatus_t;

typedef struct entry_s {
    TAILQ_ENTRY(entry_s) entries;
    entrystatus_t status;
    TAILQ_HEAD(running_s, process_info_s) running;
    char *name;
} entry_t;

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
        "   -H       Don't show hierarchy (useful for scripts)\n"
        "   -a       Show command-line of each process\n"
        "   -p       Show pid of each process\n"
        "   -t       Show thread number of each process\n"
        "   -U       Don't show uptime of the process\n"
        "   -N       Don't show name of the process\n"
        "   -r       Show RSS(Resident Set Size) of each process\n"
        "   -v       Show virtual memory size of each process\n"
        "   -u       Show CPU usage of each process (in monitor mode)\n"
        "   -o       Colorize output\n"
        "   -O       Dont' colorize output\n"
        "   -m IVL   Monitor (continuously display all the processes with\n"
        "            interval IVL in seconds\n"
        "\n"
        );
}

int parse_options(int argc, char **argv, bosstree_opt_t *options) {
    int opt;
    while((opt = getopt(argc, argv, "AC:P:dcHahpoOm:NrtuUv")) != -1) {
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
        case 'v': options->show_vsize = TRUE; break;
        case 'u': options->show_cpu = TRUE; break;
        case 'm': options->monitor = strtod(optarg, NULL); break;
        case 'o': options->color = TRUE; break;
        case 'O': options->color = FALSE; break;
        case 'H': options->show_hier = FALSE; break;
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

int parse_child_entry(int pid, char *data, int dlen, process_info_t *info) {
    assert(dlen > 1);
    char cfgpath[dlen+1];
    char pname[dlen+1];
    int bosspid;
    int childpid;
    if(sscanf(data, "%[^,],%d,%[^,],%d",
        cfgpath, &bosspid, pname, &childpid) != 4) {
        return FALSE;
    }
    if(!info->bossconfig) {
        info->bossconfig = strdup(cfgpath);
    }
    info->bosspid = bosspid;
    info->name = strdup(pname);
    info->mypid = childpid;
    if(info->pid == info->mypid) {
        info->kind = DETACHED;
    } else {
        info->kind = DETACHED_ANCESTOR;
    }
    return TRUE;
}

int parse_boss_entry(int pid, char *data, int dlen, process_info_t *info) {
    assert(dlen > 1);
    char cfgpath[dlen+1];
    int bosspid;
    if(sscanf(data, "%[^,],%d",
        cfgpath, &bosspid) != 2) {
        return FALSE;
    }
    if(info->bossconfig) free(info->bossconfig);
    info->bossconfig = strdup(cfgpath);
    info->mypid = bosspid;
    char *name = strrchr(info->cmd, '/');
    if(!name) name = info->cmd;
    else name += 1;
    info->name = strdup(name);
    if(info->pid == bosspid) {
        info->kind = BOSS;
    }
    return TRUE;
}

int parse_stat(int pid, process_info_t *info, bosstree_opt_t *options) {
    char filename[64];
    sprintf(filename, "/proc/%d/stat", pid);
    int fd = open(filename, O_RDONLY);
    if(fd < 0) return FALSE;
    char buf[4096];
    int apid;
    int len = read(fd, buf, 4095);
    assert(len >= 0);
    struct timeval tm;
    gettimeofday(&tm, NULL);
    buf[len] = 0;
    long utime, stime, cutime, cstime;
    sscanf(buf,
        "%d (%*[^)]) %*c %d %d %d"
        " %d %d %*u %*u %*u %*u %*u"
        " %lu %lu %ld %ld"
        " %*d %*d %ld %*d %lld %lu %ld",
        &apid, &info->ppid, &info->pgid, &info->psid,
        &info->tty, &info->tpgid,
        &utime, &stime, &cutime, &cstime,
        &info->threads, &info->starttime, &info->vsize, &info->rss);
    info->cpu_time = utime+stime;
    info->up_time = (tm.tv_sec - options->boottime)*options->jiffie
        + tm.tv_usec * options->jiffie / 1000000 - info->starttime;
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
    if(len) {
        info->cmd = malloc(len);
        assert(info->cmd);
        memcpy(info->cmd, buf, len);
        info->cmd[len-1] = 0;
        info->cmd_len = len;
    }
    close(fd);
    return TRUE;
}

int parse_environ(int pid, process_info_t *info) {
    char filename[64];
    sprintf(filename, "/proc/%d/environ", pid);
    FILE *f = fopen(filename, "r");
    if(!f) return FALSE;
    ssize_t linelen;
    size_t buflen = 0;
    char *line = NULL;
    while((linelen = getdelim(&line, &buflen, 0, f)) > 0) {
        if(!strncmp(line, "BOSS_CHILD=", 11)) {
            if(!parse_child_entry(pid, line + 12, strlen(line + 12), info)) {
                free(line);
                fclose(f);
                return FALSE;
            }
        } else if(!strncmp(line, "BOSSD=", 6)
               || !strncmp(line, "BOSSRUN=", 8)){
            char *pos = strchr(line, '=')+1;
            if(!parse_boss_entry(pid, pos, strlen(pos), info)) {
                free(line);
                fclose(f);
                return FALSE;
            }
        }
    }
    free(line);
    fclose(f);
    return TRUE;
}

int find_processes(process_info_t **res, bosstree_opt_t *options) {
    int size = 2;
    int cur = 0;
    process_info_t *processes = malloc(sizeof(process_info_t)*size);
    assert(processes);

    DIR *dir = opendir("/proc");
    if(!dir) return 0;
    struct dirent *entry;
    while((entry = readdir(dir))) {
        if(cur >= size) {
            size *= 2;
            processes = realloc(processes, sizeof(process_info_t)*size);
        }
        int pid = strtol(entry->d_name, NULL, 10);
        if(!pid) continue;
        memset(processes+cur, 0, sizeof(process_info_t));
        processes[cur].pid = pid;
        if(!parse_stat(pid, &processes[cur], options))
            continue;
        if(!parse_arguments(pid, &processes[cur]))
            continue;
        if(!parse_environ(pid, &processes[cur])) {
            if(processes[cur].cmd) {
                free(processes[cur].cmd);
            }
            continue;
        }
        ++cur;
    }
    closedir(dir);
    *res = processes;
    return cur;
}

void print_proc(process_info_t *child, bosstree_opt_t *options, int color) {
    int started = FALSE;
#define COLOR(a) if(!color && options->color) { printf("\033[%dm", (a)); }
#define COMMA if(started) { putchar(','); } else { started = TRUE; }
    if(color && options->color) {
        printf("\033[%dm", color);
    }
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
        int uptime = child->up_time / options->jiffie;
        if(uptime > 86400) {
            printf("up %dd%dh%dm%ds", uptime / 86400,
                uptime/3600 % 24, uptime/60 % 60 , uptime % 60);
        } else if(uptime > 3600) {
            printf("up %dh%dm%ds", uptime/3600, uptime/60 % 60 , uptime % 60);
        } else if(uptime > 60) {
            printf("up %dm%ds", uptime / 60 , uptime % 60);
        } else {
            printf("up %ds", uptime);
        }

        COLOR(FORE_RESET);
    }
    if(options->show_threads) {
        COLOR(FORE_BLUE);
        COMMA;
        printf("%ld threads", child->threads);
        COLOR(FORE_RESET);
    }
    if(options->show_cpu) {
        COLOR(FORE_BLUE)
        COMMA;
        if(child->cpu) {
            printf("%4.1f%%", child->cpu*100);
        } else {
            printf("-- %%");
        }
        COLOR(FORE_RESET);
    }
    if(options->show_rss) {
        COLOR(FORE_BLUE)
        COMMA;
        long rss = child->rss * options->pagesize;
        if(rss > 10000000000) {
            printf("%ldGiB", rss >> 30);
        } else if(rss > 1000000000) {
            printf("%3.1fGiB", (double)rss/(1 << 30));
        } else if(rss > 10000000) {
            printf("%ldMiB", rss >> 20);
        } else {
            printf("%3.1fMiB", (double)rss/(1 << 20));
        }
        COLOR(FORE_RESET);
    }
    if(options->show_vsize) {
        COLOR(FORE_BLUE)
        COMMA;
        long rss = child->vsize;
        if(rss > 10000000000) {
            printf("v%ldGiB", rss >> 30);
        } else if(rss > 1000000000) {
            printf("v%3.1fGiB", (double)rss/(1 << 30));
        } else if(rss > 10000000) {
            printf("v%ldMiB", rss >> 20);
        } else {
            printf("v%3.1fMiB", (double)rss/(1 << 20));
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
    if(color && options->color) {
        printf("\033[%dm", FORE_RESET);
    }
#undef COLOR
#undef COMMA
}

bool read_config(config_main_t *config, char *filename) {
    coyaml_context_t *ctx = config_context(NULL, config);
    ctx->root_filename = filename;
    int rc = coyaml_readfile(ctx);
    if(rc < 0) return FALSE;
    coyaml_context_free(ctx);
    return TRUE;
}

void print_processes(process_info_t *tbl, int num, bosstree_opt_t *options) {
    char prefix[128];
    int prefixlen = 0;
    for(int i = 0; i < num; ++i) {
        if(tbl[i].kind == BOSS) {

            print_proc(&tbl[i], options, FORE_GREEN);
            printf("\n");
            strcpy(prefix, "  │");
            prefixlen = strlen(prefix);

            entry_t *child;
            TAILQ_FOREACH(child, &tbl[i].entries, entries) {
                if(TAILQ_FIRST(&child->running)) {
                    process_info_t *proc;
                    TAILQ_FOREACH(proc, &child->running, runentry) {
                        if(options->show_hier) {
                            if(TAILQ_NEXT(proc, runentry)
                                || TAILQ_NEXT(child, entries)) {
                                printf("%.*s├─", prefixlen-3, prefix);
                            } else {
                                printf("%.*s└─", prefixlen-3, prefix);
                            }
                        }
                        if(child->status == S_STALLED
                            || child->status == S_ORPHAN) {
                            print_proc(proc, options, FORE_RED);
                        } else {
                            print_proc(proc, options, 0);
                        }
                        printf("\n");
                    }
                } else {
                    if(options->show_hier) {
                        if(TAILQ_NEXT(child, entries)) {
                            printf("%.*s├─", prefixlen-3, prefix);
                        } else {
                            printf("%.*s└─", prefixlen-3, prefix);
                        }
                    }
                    if(options->color) {
                        printf("\033[%dm%s,down\033[0m\n",
                            FORE_RED, child->name);
                    } else {
                        printf("%s,down\n", child->name);
                    }
                }
            }

        }
    }
    if(options->detached) {
        for(int i = 0; i < num; ++i) {
            if(tbl[i].kind == DETACHED) {
                process_info_t *child = &tbl[i];
                print_proc(child, options, FORE_RED);
                printf("\n");
            }
        }
    }
}

void sort_processes(process_info_t *tbl, int num) {
    for(int i = 0; i < num; ++i) {
        TAILQ_INIT(&tbl[i].children);
        TAILQ_INIT(&tbl[i].entries);
    }
    for(int i = 0; i < num; ++i) {
        for(int j = 0; j < num; ++j) {
            if(tbl[i].ppid == tbl[j].pid) {
                tbl[i].parent = &tbl[j];
                TAILQ_INSERT_TAIL(&tbl[j].children,
                    &tbl[i], chentry);
                break;
            }
        }
    }

    for(int i = 0; i < num; ++i) {
        if(tbl[i].kind != BOSS) continue;
        config_main_t config;
        if(!read_config(&config, tbl[i].bossconfig)) {
            fprintf(stderr, "Can't read config ``%s'': %m", tbl[i].bossconfig);
        }
        CONFIG_STRING_PROCESS_LOOP(item, config.Processes) {
            entry_t *entry = malloc(sizeof(entry_t));
            entry->status = S_DOWN;
            TAILQ_INSERT_TAIL(&tbl[i].entries, entry, entries);
            entry->name = strdup(item->key);
            TAILQ_INIT(&entry->running);
            for(process_info_t *child = TAILQ_FIRST(&tbl[i].children);
                child; child=TAILQ_NEXT(child, chentry)) {
                if(strcmp(child->name, item->key))
                    continue;
                TAILQ_INSERT_TAIL(&entry->running, child, runentry);
                entry->status = S_NORMAL;
            }
        }
        config_free(&config);
        for(process_info_t *child = TAILQ_FIRST(&tbl[i].children);
            child; child=TAILQ_NEXT(child, chentry)) {
            if(!child->bosspid) {
                entry_t *entry = malloc(sizeof(entry_t));
                entry->status = S_ORPHAN;
                TAILQ_INSERT_TAIL(&tbl[i].entries, entry, entries);
                entry->name = NULL;
                TAILQ_INIT(&entry->running);
                TAILQ_INSERT_TAIL(&entry->running, child, runentry);
            } else {
                int found = FALSE;
                entry_t *entry;
                TAILQ_FOREACH(entry, &tbl[i].entries, entries) {
                    if(!strcmp(entry->name, child->name)) {
                        process_info_t *proc;
                        TAILQ_FOREACH(proc, &entry->running, runentry) {
                            if(proc == child) {
                                found = TRUE;
                                goto break2;
                            }
                        }
                    }
                }
                break2:
                if(!found) {
                    entry = malloc(sizeof(entry_t));
                    entry->status = S_STALLED;
                    TAILQ_INSERT_TAIL(&tbl[i].entries, entry, entries);
                    entry->name = child->name;
                    TAILQ_INSERT_TAIL(&entry->running, child, runentry);
                }
                if(child->bosspid == tbl[i].pid) {
                    if(child->pid == child->mypid) {
                        child->kind = BOSS_CHILD;
                    } else {
                        child->kind = BOSS_ANCESTOR;
                    }
                }
            }
        }
    }
}

void compare_processes(process_info_t *tbl1, int num1,
                       process_info_t *tbl2, int num2) {
    for(int i = 0; i < num1; ++i) {
        for(int j = 0; j < num2; ++j) {
            if(tbl1[i].pid == tbl2[j].pid) {
                tbl1[i].cpu = (double)(tbl1[i].cpu_time - tbl2[j].cpu_time)
                    / (tbl1[i].up_time - tbl2[j].up_time);
                break;
            }
        }
    }
}

void free_processes(process_info_t *tbl, int num) {
    for(int i = 0; i < num; ++i) {
        if(tbl[i].cmd) free(tbl[i].cmd);
        if(tbl[i].bossconfig) free(tbl[i].bossconfig);
        if(tbl[i].name != tbl[i].cmd) free(tbl[i].name);
        for(entry_t *nxt, *cur = TAILQ_FIRST(&tbl[i].entries); cur; cur=nxt) {
            nxt = TAILQ_NEXT(cur, entries);
            if(cur->name) {
                free(cur->name);
            }
            free(cur);
        }
    }
    free(tbl);
}

int main(int argc, char **argv) {
    bosstree_opt_t options = {
        all: TRUE,
        config: NULL,
        pid: 0,
        detached: FALSE,
        children: FALSE,
        show_hier: TRUE,
        show_cmd: FALSE,
        show_pid: FALSE,
        show_threads: FALSE,
        show_uptime: TRUE,
        show_name: TRUE,
        show_rss: FALSE,
        show_vsize: FALSE,
        show_cpu: FALSE,
        color: isatty(1),
        monitor: 0.0,
        jiffie: sysconf(_SC_CLK_TCK),
        pagesize: sysconf(_SC_PAGE_SIZE),
        boottime: get_boot_time()
        };
    parse_options(argc, argv, &options);
    process_info_t *tbl;
    int num;
    if(options.monitor) {
        struct timespec tm = {
            tv_sec: (int)options.monitor,
            tv_nsec: (int)(1000000000*options.monitor) % 1000000000
            };
        if(options.color) {
            printf("\033[1J");
        }
        num = find_processes(&tbl, &options);
        sort_processes(tbl, num);
        print_processes(tbl, num, &options);
        while(1) {
            if(options.color) {
                printf("\033[1J");
            } else {
                printf("----------\n");
            }
            process_info_t *tbl2;
            int num2 = find_processes(&tbl2, &options);
            compare_processes(tbl2, num2, tbl, num);
            free_processes(tbl, num);
            tbl = tbl2;
            num = num2;
            sort_processes(tbl, num);
            print_processes(tbl, num, &options);
            nanosleep(&tm, NULL);
        }
    } else {
        num = find_processes(&tbl, &options);
        sort_processes(tbl, num);
        print_processes(tbl, num, &options);
        free_processes(tbl, num);
    }

    return 0;
}
