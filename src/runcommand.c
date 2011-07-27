#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>

#include "runcommand.h"

#define CHECK(res, msg) if((res) < 0) { perror(msg); abort(); }

static void open_files(config_process_t *process) {
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
            CHECK(gr ? -1 : 0, "Can't find group");
            gid = gr->gr_gid;
        }
        CHECK(setgid(gid), "Can't set gid");
    }
    if(process->user_len) {
        char *end;
        int uid = strtol(process->user, &end, 0);
        if(end != process->user + process->user_len) {
            struct passwd *pw = getpwnam(process->user);
            CHECK(pw ? -1 : 0, "Can't find user");
            uid = pw->pw_uid;
        }
        CHECK(setuid(uid), "Can't set uid");
    }
    if(process->chroot) {
        CHECK(chroot(process->chroot), "Can't chroot");
    }
}

int fork_and_run(config_process_t *process) {
    int res = fork();
    if(res < 0)
        return -1;  // Error occured
    if(res > 0)
        return res; // We are parent, just return pid

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
    char *environ[process->environ_len+1];
    int i = 0;
    CONFIG_STRING_LOOP(item, process->arguments) {
        argv[i++] = item->value;
    }
    argv[i] = NULL;
    i = 0;
    CONFIG_STRING_STRING_LOOP(item, process->environ) {
        char *data = alloca(item->key_len + item->value_len + 2);
        strcpy(data, item->key);
        strcat(data, "=");
        strcat(data, item->value);
        environ[i++] = data;
    }
    environ[i] = NULL;

    CHECK(execve(process->executable_path, argv, environ),
        "Can't execute");
    return 0;
}

