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

const char *levels[] = {  // PLEASE keep in sync with loglevel_t
    "CRASH",  // for crashes of boss itself
    "CRITICAL",  // boss can't operate normally
    "RECOVER",  // recovering processes after inplace restart
    "CONTROL",  // commands throught control interface
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

void rotatelogs() {
    char filename1[config.bossd.logging.file_len + 32];
    char filename2[config.bossd.logging.file_len + 32];
    struct stat stinfo;
    int i;
    for(i = 1; i < config.bossd.logging.rotation_backups+1; ++i) {
        sprintf(filename1, "%s.%d", config.bossd.logging.file, i);
        if(stat(filename1, &stinfo) == -1) {
            if(errno != ENOENT)
                STDASSERT2(-1, "Can't stat log backups");
            break;
        }
    }
    for(; i > 1; --i) {
        sprintf(filename2, "%s.%d", config.bossd.logging.file, i-1);
        STDASSERT2(rename(filename2, filename1), "Can't move log backup");
        strcpy(filename1, filename2);
    }
    sprintf(filename1, "%s.%d", config.bossd.logging.file, 1);
    STDASSERT2(rename(config.bossd.logging.file, filename1),
        "Can't move log to a backup")
    reopenlogs();
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

off_t logsize() {
    return lseek(logfile, 0, SEEK_CUR);
}
