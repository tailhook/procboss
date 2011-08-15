#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"

long get_boot_time() {
    char buf[16384];
    int fd = open("/proc/stat", O_RDONLY);
    assert(fd >= 0);
    int len = read(fd, buf, 16383);
    assert(len > 0);
    buf[len] = 0;
    char *btime = strstr(buf, "btime ");
    assert(btime);
    long res = strtol(btime+6, NULL, 10);
    close(fd);
    return res;
}
