=======
bossctl
=======

-------------------------
control utility for bossd
-------------------------

:Author: Paul Colomiets <paul@colomiets.name>
:Date: 2011
:Manual section: 8

Synopsis
--------

| bossctl [ -c *FILE* ] *command* *args...*

Description
-----------

Bossctl is an utility to send bossd control commands including process control
and configuration reload.

Options
-------

  -c CONFIG_FILE
    configuration file. Default is /etc/bossd.yaml

Process Control Commands
------------------------

restart *PROCESS1* *PROCESS2*...

    restarts specified processes by name (sending it SIGTERM)

stop *PROCESS1* *PROCESS2*...

    stops process by sending it SIGTERM, and marking process as down. Process
    will be started again only on next restart or if started manually

start *PROCESS1* *PROCESS2*...

    starts processes, even if they are were crashed too much and sleeping big
    timeout. Also this command marks process to be restarted after death

startin /dev/pts/*NUM* *PROCESS*

    starts process with the specified tty as controlling terminal (used
    internally for show command)

show *PROCESS*

    starts process in current terminal (internally works by creating new pty
    and proxying). Proccess need to be down when running this command.

sigterm *PROCESS1* *PROCESS2*...

sighup *PROCESS1* *PROCESS2*...

sigkill *PROCESS1* *PROCESS2*...

sigusr1 *PROCESS1* *PROCESS2*...

sigusr2 *PROCESS1* *PROCESS2*...

sigint *PROCESS1* *PROCESS2*...

sigquit *PROCESS1* *PROCESS2*...

    sends respective signal to a process. If process dies it's death is
    recorded as all other, so expect process to banned when you kill to often

sig *NUM* *PROCESS1* *PROCESS2*...

    sends a signal by a number (useful to send signals having to separate
    command, like SIGSEGV)

norestart *NUM* *PROCESS1* *PROCESS2*...

    marks process (instance) to not to restart if it crashed. Useful with
    ``-p`` flag and subsequent call to ``sig*`` commands to stop exactly
    specified instance of process instead of using ``decr`` and ``min`` (see
    below)

Instance Manipulation Commands
------------------------------

incr *PROCESS1* *PROCESS2*...

    increment number of instances for each of the processes

decr *PROCESS1* *PROCESS2*...

    decrement number of instances for each of the processes. Kills ones having
    biggest instance number first. Kills with ``spare-kill-signal`` from
    configuration (SIGTERM by default)

max *PROCESS1* *PROCESS2*...

    start maximum allowed number of instances for each process

min  *PROCESS1* *PROCESS2*...

    kill some instances of the process, until minimum number left. Kills ones
    having biggest instance number first. Kills with ``spare-kill-signal`` from
    configuration (SIGTERM by default)

Process Matching
----------------

By default you specify exact process names as specified in configuration
file. All process control commands support following options:

-a,-A    match (no match) command arguments

-b,-B    match (no match) executable path

-p,-P    match (no match) pid of running processes

-n,-N    match (no match) configured process name (on by default)

-e,-E    match by fnmatch pattern (exact match)

Bossd Global Commands
---------------------

shutdown

    shut down bossd and all started processes

explode

    do in-place restart of bossd and recover child process' info (useful for
    configuration reloading and updating of bossd binary)

reload

    currently mapped to ``explode``, will probably be implemented in future as
    as a more pretty way to reload configuration

reopenlog

    reopen log files, in case some external tool removed or rotated it

rotatelog

    force log rotation
