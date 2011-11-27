#!/usr/bin/python
# trying to keep it both python2 and python3 compatible
"""
Example of SCGI backend server

It's designed to be short example. But we are still trying to handle all the
exceptions properly. Some ineffeciencies are also acceptable.
"""
import socket
import errno
import pprint
import os
import sys
from contextlib import closing

# we have a file descriptor passed to us from bossd, because we want it
# shared between several workers
mainsock = socket.fromfd(0, socket.AF_INET, socket.SOCK_STREAM)

while True:
    try:
        sock, addr = mainsock.accept()
    except socket.error as e:
        if e.errno in (errno.EINTR, errno.EAGAIN):
            continue
        raise
    else:
        with closing(sock):  # compatibility with python2.7
            buf = b""
            while True:
                try:
                    chunk = sock.recv(4906)
                except socket.error as e:
                    if e.errno in (errno.EINTR, errno.EAGAIN):
                        continue
                    raise
                if not chunk:
                    print("Incomplete data in connection")
                    break
                buf += chunk
                try:
                    idx = buf.index(b':')
                    dlen = int(buf[:idx])
                except IndexError:
                    continue
                except ValueError:
                    print("Wrong length data")
                    break
                if len(buf) < dlen + idx + 1:
                    continue
                pairs = iter(buf[idx+1:idx+dlen+1].split(b'\0'))
                headers = dict(zip(pairs, pairs))
                try:
                    clen = int(headers[b'CONTENT_LENGTH'])
                except (KeyError, ValueError):
                    continue
                if len(buf) < dlen + idx + 2 + clen:
                    continue
                data = buf[dlen+idx+2:]
                if len(data) > clen:
                    print("Excessive data in connection")
                    break

                # Got everything parsed OK
                print("Processing", headers[b'REQUEST_URI'])
                reply = (
                    b'Status: 200 OK\r\n'
                    b'Content-Type: text/plain\r\n'
                    b'\r\n'
                    b'SCGI headers:\r\n'
                    + pprint.pformat(headers).encode('ascii')
                    + b'\r\nSCGI body:\r\n'
                    + repr(data).encode('ascii')
                    + b'\r\nEnviron:\r\n'
                    + pprint.pformat(os.environ).encode('ascii')
                    + b'\r\nArgs:\r\n'
                    + repr(sys.argv).encode('ascii')
                    + b'\r\nPid:' + str(os.getpid()).encode('ascii')
                    )
                try:
                    sock.sendall(reply)
                except socket.error:
                    # let socket be closed
                    pass
                break

