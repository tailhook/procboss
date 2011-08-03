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
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "runcommand.h"

extern char *configuration_name;
extern int configuration_name_len;

static inline int CHECK(int res, char *msg) {
    if(res < 0) {
        perror(msg);
        abort();
    }
    return res;
}

static int open_tcp(config_file_t *file) {
    int tfd = CHECK(socket(AF_INET, SOCK_STREAM, 0),
        "Can't create socket");
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_aton(file->host, &addr.sin_addr);
    addr.sin_port = htons(file->port);
    int i = 1;
    CHECK(setsockopt(tfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)),
        "Can't set socket option");
    CHECK(bind(tfd, &addr, sizeof(addr)),
        "Can't bind to {{ file.host }}:{{ file.port }}");
    CHECK(listen(tfd, SOMAXCONN),
        "Can't listen on {{ file.host }}:{{ file.port }}");
    return tfd;
}

static int open_unix(config_file_t *file) {
    int tfd = CHECK(socket(AF_UNIX, SOCK_STREAM, 0),
        "Can't create socket");
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, file->path);
    int addr_len = file->path_len + 1 + sizeof(addr.sun_family);
    if(connect(tfd, (struct sockaddr *)&addr, addr_len) != -1) {
        CHECK(-1, "Socket still used");
    }
    switch(errno) {
    case ECONNREFUSED: unlink(file->path); break;
    case ENOENT: break; // OK
    default: CHECK(-1, "Error probing socket");
    }
    CHECK(bind(tfd, &addr, addr_len), "Can't bind to path");
    if(file->mode) {
        CHECK(fchmod(tfd, file->mode),
            "Can't set file mode"); // fchmod does not work
    }
    // TODO(tailhook) set user and group
    CHECK(listen(tfd, SOMAXCONN), "Can't listen");
    return tfd;
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
            tfd = open_tcp(&item->value);
        } else if(item->value.type == CONFIG_UnixSocket) {
            tfd = open_unix(&item->value);
        } else if(item->value.type == CONFIG_File) {
            tfd = open_file(&item->value);
        } else {
            assert(0);
        }
        if(tfd != item->key) {
            CHECK(dup2(tfd, item->key), "Can't dup file descriptor");
            CHECK(close(tfd), "Can't close file descriptor");
        }

    }
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

int fork_and_run(config_process_t *process) {
    int i;
    int parentpid = getpid();
    int res = fork();
    if(res < 0) {
        process->_entry.status = PROC_ERROR;
        return -1;  // Error occured
    }
    if(res > 0) {
        process->_entry.status = PROC_STARTING;
        process->_entry.pid = res;
        if(process->_entry.pending == PENDING_RESTART) {
            process->_entry.pending = PENDING_UP;
        }
        live_processes += 1;
        return res; // We are parent, just return pid
    }
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
        argv[i++] = item->value;
    }
    argv[i] = NULL;

    char *environ[process->environ_len+2];
    char **tmpenv = environ;
    char bossenv[process->_name_len + configuration_name_len + 64];
    sprintf(bossenv, "BOSS_CHILD=%s,%d,%s,%d",
        configuration_name, parentpid, process->_name, getpid());
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
    return 0;
}

