#define _BSD_SOURCE
#define _GNU_SOURCE
#include "log.h"
#include "config.h"
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

const char *levels[] = {
    "CRASH",  // for crashes of boss itself
    "CRITICAL",  // boss can't operate normally
    "RECOVER",  // recovering processes after inplace restart
    "WARNING",  // various warnings
    "DEATH",  // death of a child (which is not a big deal)
    "STARTUP",  // start of a new child
    NULL};

int logfile = 2;

const int STDOUT = 2;
const int STDERR = 3;

extern config_main_t config;

void logmsg(int level, char *msg, ...) {
    char buf[4096];
    time_t tm;
    struct tm ts;
    va_list args;
    int pid = getpid();  // its fast to call in linux
    time(&tm);
    localtime_r(&tm, &ts);
    int idx = snprintf(buf, 4096, "%04d-%02d-%02d %02d:%02d:%02d [%d] %s: ",
        ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday,
        ts.tm_hour, ts.tm_min, ts.tm_sec, pid, levels[level]);
    va_start(args, msg);
    idx += vsnprintf(buf + idx, 4096 - idx, msg, args);
    if(idx > 4094) {
        idx = 4094;
    }
    buf[idx++] = '\n';
    write(logfile, buf, idx);
}

void logstd(int level, char *msg, ...) {
    char buf[4096];
    time_t tm;
    struct tm ts;
    va_list args;
    int pid = getpid();  // its fast to call in linux
    int local_errno = errno;
    time(&tm);
    localtime_r(&tm, &ts);
    int idx = snprintf(buf, 4096,
        "%04d-%02d-%02d %02d:%02d:%02d [%d] %s: (e%d)",
        ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday,
        ts.tm_hour, ts.tm_min, ts.tm_sec,
        pid, levels[level], local_errno);
    va_start(args, msg);
    idx += vsnprintf(buf + idx, 4096 - idx, msg, args);
    if(idx < 4093) {
        buf[idx++] = ':';
        buf[idx++] = ' ';
        char *res = strerror_r(local_errno, buf + idx, 4096-idx);
        if(res != buf+idx) {
            strncpy(buf+idx, res, 4096-idx);
        }
        idx += strlen(buf + idx);
    }
    if(idx > 4094) {
        idx = 4094;
    }
    buf[idx++] = '\n';
    write(logfile, buf, idx);
}

void setcloexec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    STDASSERT(flags);
    fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

void openlogs() {
    int oldlogfile = logfile;
    logfile = open(config.bossd.logging.file, O_APPEND|O_WRONLY|O_CREAT,
        config.bossd.logging.mode);
    if(logfile <= 0) {
        logfile = oldlogfile;
        STDWARN("Can't open logfile");
        abort();
    } else {
        LWARNING("Log sucessfully opened");
        setcloexec(logfile);
    }
}

void reopenlogs() {
    int oldlogfile = logfile;
    logfile = open(config.bossd.logging.file, O_APPEND|O_WRONLY|O_CREAT,
        config.bossd.logging.mode);
    if(logfile <= 0) {
        logfile = oldlogfile;
        STDWARN("Can't open logfile. Continuing writing old file");
    } else {
        LWARNING("New log file successfully opened");
        setcloexec(logfile);
        close(oldlogfile);
    }
}
