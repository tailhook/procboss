/*
 * This code is stolen from reptyr with the following copyright:
 *
 * Copyright (C) 2011 by Nelson Elhage
 *
 * Modified by Paul Colomiets (c) 2011
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#define _XOPEN_SOURCE
#define _BSD_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <termios.h>
#include <signal.h>

#define CHECK(v, msg) if((v) < 0) { perror((msg)); abort(); }

void setup_raw(struct termios *save) {
    struct termios set;
    CHECK(tcgetattr(0, save), "Unable to read terminal attributes: %m");
    set = *save;
    cfmakeraw(&set);
    CHECK(tcsetattr(0, TCSANOW, &set),
        "Unable to set terminal attributes: %m");
}

void resize_pty(int pty) {
    struct winsize sz;
    if (ioctl(0, TIOCGWINSZ, &sz) < 0)
        return;
}

int writeall(int fd, const void *buf, ssize_t count) {
    ssize_t rv;
    while (count > 0) {
        rv = write(fd, buf, count);
        if (rv < 0)
            return rv;
        count -= rv;
        buf += rv;
    }
    return 0;
}

int winch_happened = 0;

void do_winch(int signal) {
    winch_happened = 1;
}

void describe_timeout() {
    fprintf(stderr,
        "Timed out waiting for process start.\n"
        "Either bossd can't start process or process is already running\n"
        "In the latter case just stop process before running show command\n");
}

int do_proxy(int pty) {
    char buf[4096];
    ssize_t count;
    int has_process = 0;
    fd_set set;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    while (1) {
        if (winch_happened) {
            resize_pty(pty);
            /* FIXME: racy against a second resize */
            winch_happened = 0;
        }
        FD_ZERO(&set);
        FD_SET(0, &set);
        FD_SET(pty, &set);
        int fdno = select(pty+1, &set, NULL, NULL, has_process ? NULL : &tv);
        if (fdno < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "select: %m");
            return 1;
        }
        if(fdno == 0) {
            if(tcgetpgrp(pty) <= 0) {
                return 2;
            }
            has_process = 1;
        }
        if (FD_ISSET(0, &set)) {
            count = read(0, buf, sizeof buf);
            if (count < 0)
                return 0;
            writeall(pty, buf, count);
        }
        if (FD_ISSET(pty, &set)) {
            count = read(pty, buf, sizeof buf);
            if (count < 0)
                return 0;
            writeall(1, buf, count);
        }
    }
    return 1;
}


int make_pty() {
    int pty;

    CHECK(pty = open("/dev/ptmx", O_RDWR|O_NOCTTY),
        "Unable to open /dev/ptmx: %m");
    CHECK(unlockpt(pty), "Unable to unlockpt: %m");
    CHECK(grantpt(pty), "Unable to grantpt: %m");
    return pty;
}

void start_proxy(int pty) {
    struct termios saved_termios;
    struct sigaction act;

    setup_raw(&saved_termios);
    resize_pty(pty);
    memset(&act, 0, sizeof act);
    act.sa_handler = do_winch;
    act.sa_flags   = 0;
    sigaction(SIGWINCH, &act, NULL);
    int res = do_proxy(pty);
    tcsetattr(0, TCSANOW, &saved_termios);
    if(res == 2) {
        describe_timeout();
    }

    exit(res);
}
