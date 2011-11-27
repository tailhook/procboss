#define _GNU_SOURCE
#define _POSIX_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "runcommand.h"
#include "log.h"
#include "sockets.h"

extern char *configuration_name;
extern int configuration_name_len;

static inline int CHECK(int res, char *msg) {
    if(res < 0) {
        logstd(LOG_STARTUP, msg);
        exit(127);
    }
    return res;
}

static int open_file(config_file_t *file) {
    int flags = 0;
    if(!strcmp(file->flags, "r")) {
        flags = O_RDONLY;
    } else if(!strcmp(file->flags, "w")) {
        flags = O_WRONLY|O_CREAT|O_TRUNC;
    } else if(!strcmp(file->flags, "a")) {
        flags = O_WRONLY|O_CREAT|O_APPEND;
    } else if(!strcmp(file->flags, "r+")) {
        flags = O_RDWR;
    } else if(!strcmp(file->flags, "w+")) {
        flags = O_RDWR|O_CREAT|O_TRUNC;
    } else {
        CHECK(-1, "Wrong file flags");
    }
    int tfd = CHECK(open(file->path, flags, file->mode),
        "Can't open file");
    if(file->mode) {
        CHECK(fchmod(tfd, file->mode),
            "Can't set file mode");
    }
    // TODO(tailhook) set user and group of file

    return tfd;
}

static void open_files(config_process_t *process) {
    CONFIG_LONG_FILE_LOOP(item, process->files) {
        int tfd;
        if(item->value.type == CONFIG_Tcp) {
            if(item->value._fd >= 0) {
                tfd = item->value._fd;
            } else {
                tfd = open_tcp(&item->value, TRUE);
            }
        } else if(item->value.type == CONFIG_UnixSocket) {
            if(item->value._fd >= 0) {
                tfd = item->value._fd;
            } else {
                tfd = open_unix(&item->value, TRUE);
            }
        } else if(item->value.type == CONFIG_File) {
            tfd = open_file(&item->value);
        } else {
            assert(0);
        }
        if(tfd < 0) {
            CHECK(tfd, "Error opening file");
        }
        if(tfd != item->key) {
            CHECK(dup2(tfd, item->key), "Can't dup file descriptor");
            CHECK(close(tfd), "Can't close file descriptor");
        }

    }
    close_all(process);
}

static void set_limit(int resource, unsigned long value) {
    struct rlimit limit = { value, value };
    setrlimit(resource, &limit);
}

static void set_limits(config_process_t *process)
{
    if(process->limits.core)
        set_limit(RLIMIT_CORE, process->limits.core);
    if(process->limits.cpu)
        set_limit(RLIMIT_CPU, process->limits.cpu);
    if(process->limits.data)
        set_limit(RLIMIT_DATA, process->limits.data);
    if(process->limits.fsize)
        set_limit(RLIMIT_FSIZE, process->limits.fsize);
    if(process->limits.nofile)
        set_limit(RLIMIT_NOFILE, process->limits.nofile);
    if(process->limits.stack)
        set_limit(RLIMIT_STACK, process->limits.stack);
    if(process->limits.as)
        set_limit(RLIMIT_AS, process->limits.as);
}

static void drop_privileges(config_process_t *process) {
    if(getuid() > 0)
        return;
    if(process->group_len) {
        char *end;
        int gid = strtol(process->group, &end, 0);
        if(end != process->group + process->group_len) {
            struct group *gr = getgrnam(process->group);
            CHECK(gr ? 0 : -1, "Can't find group");
            gid = gr->gr_gid;
        }
        CHECK(setgid(gid), "Can't set gid");
    }
    if(process->user_len) {
        char *end;
        int uid = strtol(process->user, &end, 0);
        if(end != process->user + process->user_len) {
            struct passwd *pw = getpwnam(process->user);
            CHECK(pw ? 0 : -1, "Can't find user");
            uid = pw->pw_uid;
        }
        CHECK(setuid(uid), "Can't set uid");
    }
    if(process->chroot_len) {
        CHECK(chroot(process->chroot), "Can't chroot");
    }
}


int do_fork(config_process_t *process, int *instance_index) {
    process_entry_t *entry = malloc(sizeof(process_entry_t));
    if(!entry) return -1;

    int idx = 0;
    process_entry_t *inspoint;
    int found = FALSE;
    CIRCLEQ_FOREACH(inspoint, &process->_entries.entries, cq) {
        if(inspoint->instance_index > idx) {
            found = TRUE;
            break;
        } else {
            idx = inspoint->instance_index+1;
        }
    }
    if(!found) {
        inspoint = NULL;
    }
    *instance_index = idx;

    int res = fork();
    if(res < 0) {
        free(entry);
        return -1;  // Error occured
    }
    if(res > 0) {
        struct timeval tm;
        gettimeofday(&tm, NULL);
        process->_entries.running += 1;
        entry->pid = res;
        process->_entries.last_start_time = entry->start_time = TVAL2DOUBLE(tm);
        entry->config = process;
        entry->all = &process->_entries;
        entry->dead = DEAD_CRASH;
        entry->instance_index = idx;
        if(inspoint) {
            CIRCLEQ_INSERT_BEFORE(&process->_entries.entries,
                inspoint, entry, cq);
        } else {
            CIRCLEQ_INSERT_TAIL(&process->_entries.entries, entry, cq);
        }
        live_processes += 1;
        LSTARTUP("Started \"%s\":%d with pid %d", process->_name, idx, res);
        return res; // We are parent, just return pid
    }
    return res;
}

// very primitive substitution function
static char *substitute(char *str, int val) {
    char seq0[11];
    char seq1[11];
    sprintf(seq0, "%d", val);
    sprintf(seq1, "%d", val+1);

    int curlen = strlen(str)+1;
    char *output = malloc(curlen);
    char *p = output;
    for(char *c = str; *c; ++c) {
        if(*c == '@') {
            char *value;
            int strip;
            if(!strncmp(c, "@(seq0)", 7)) {
                value = seq0;
                strip = 7;
            } else if(!strncmp(c, "@(seq1)", 7)) {
                value = seq1;
                strip = 7;
            } else {
                *p = *c;
                continue;
            }
            char *no = realloc(output, curlen-strip+strlen(seq0));
            p = no + (p - output);
            output = no;
            strcpy(p, value);
            p += strlen(value);
            c += strip-1;
        } else {
            *p++ = *c;
        }
    }
    *p = 0;
    return output;
}

void do_run(config_process_t *process, int parentpid, int instance_index) {
    int i;
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    open_files(process);
    set_limits(process);
    drop_privileges(process);

    if(process->umask > 0) {
        CHECK(umask(process->umask), "Can't set umask");
    }
    if(process->work_dir_len) {
        CHECK(chdir(process->work_dir), "Can't set workdir");
    }

    char *argv[process->arguments_len+1];
    i = 0;
    CONFIG_STRING_LOOP(item, process->arguments) {
        if(process->enable_subst) {
            argv[i++] = substitute(item->value, instance_index);
        } else {
            argv[i++] = item->value;
        }
    }
    argv[i] = NULL;

    char *environ[process->environ_len+2];
    char **tmpenv = environ;
    char bossenv[process->_name_len + configuration_name_len + 64];
    sprintf(bossenv, "BOSS_CHILD=%s,%d,%s,%d,%d",
        configuration_name, parentpid,
        process->_name, getpid(), instance_index);
    *tmpenv++ = bossenv;

    CONFIG_STRING_STRING_LOOP(item, process->environ) {
        char *data = alloca(item->key_len + item->value_len + 2);
        strcpy(data, item->key);
        strcat(data, "=");
        strcat(data, item->value);
        *tmpenv++ = data;
    }
    *tmpenv = NULL;

    CHECK(execve(process->executable_path, argv, environ),
        "Can't execute");
}

int fork_and_run(config_process_t *process) {
    int parentpid = getpid();
    int index;
    int pid = do_fork(process, &index);
    if(pid < 0) return pid;
    if(!pid) {
        do_run(process, parentpid, index);
    }
    return pid;
}
