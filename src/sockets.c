#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>

#include "sockets.h"
#include "log.h"
#include "config.h"
#include "constants.h"
#include "globalstate.h"

#define CHECK(res, ...) if((res) < 0) { \
    logstd(LOG_STARTUP, ## __VA_ARGS__ ); \
    return -1; \
    }

int open_tcp(config_file_t *file, bool do_listen) {
    int tfd;
    CHECK(tfd = socket(AF_INET, SOCK_STREAM, 0),
        "Can't create socket");
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_aton(file->host, &addr.sin_addr);
    addr.sin_port = htons(file->port);
    int i = 1;
    CHECK(setsockopt(tfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)),
        "Can't set socket option");
    CHECK(bind(tfd, (struct sockaddr *)&addr, sizeof(addr)),
        "Can't bind to %s:%d", file->host, file->port);
    if(do_listen) {
        CHECK(listen(tfd, SOMAXCONN),
            "Can't listen on %s:%d", file->host, file->port);
    }
    return tfd;
}

int open_unix(config_file_t *file, bool do_listen) {
    int tfd;
    CHECK(tfd = socket(AF_UNIX, SOCK_STREAM, 0),
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
    CHECK(bind(tfd, (struct sockaddr *)&addr, addr_len), "Can't bind to path");
    if(file->mode) {
        CHECK(fchmod(tfd, file->mode),
            "Can't set file mode"); // fchmod does not work
    }
    // TODO(tailhook) set user and group
    if(do_listen) {
        CHECK(listen(tfd, SOMAXCONN), "Can't listen");
    }
    return tfd;
}

void close_all(config_process_t *process) {
    struct dirent *entry;
    DIR *dir = opendir("/proc/self/fd");
    int curfd = dirfd(dir);
    if(!dir) {
        LSTARTUP("Can't read /proc/self/fd: %m");
        return;
    }
    while((errno = 0, entry = readdir(dir))) {
        int fd = strtol(entry->d_name, NULL, 10);
        if(fd < 3 || curfd == fd) continue;
        CONFIG_LONG_FILE_LOOP(item, process->files) {
            if(item->key == fd) {
                fd = -1;
                break;
            }
        }
        if(fd >= 0) {
            close(fd);
        }
    }
    if(errno) {
        LSTARTUP("Error reading procfs: %m");
        exit(127);
    }
    closedir(dir);
}

void open_sockets(bool do_recover) {
    config_file_t *sockets[MAX_SOCKETS];
    int cur_sockets = 0;
    CONFIG_STRING_PROCESS_LOOP(process, config.Processes) {
        CONFIG_LONG_FILE_LOOP(file, process->value.files) {
            if(file->value.type != CONFIG_Tcp
                && file->value.type != CONFIG_UnixSocket)
                continue;
            for(int i = 0; i < cur_sockets; ++i) {
                if(file->value.type != sockets[i]->type) continue;
                if(file->value.type == CONFIG_Tcp) {
                    if(!strcmp(file->value.host, sockets[i]->host)
                        && file->value.port == sockets[i]->port) {
                            file->value._fd = sockets[i]->_fd;
                            break;
                    }
                }
            }
            if(file->value._fd < 0) {
                if(file->value.type == CONFIG_Tcp) {
                    file->value._fd = open_tcp(&file->value, FALSE);
                } else if(file->value.type == CONFIG_UnixSocket) {
                    file->value._fd = open_unix(&file->value, FALSE);
                }
            }
        }
    }
}
