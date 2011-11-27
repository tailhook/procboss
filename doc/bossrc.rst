======
bossrc
======

---------------------------
control utility for bossrun
---------------------------

:Author: Paul Colomiets <paul@colomiets.name>
:Date: 2011
:Manual section: 1

Synopsis
--------

| bossctl [ -c *FILE* ] *command* *args...*

Description
-----------

Bossrc is an utility to send bossrun control commands including process
control

Options
-------

  -c CONFIG_FILE
    configuration file. Default is /etc/bossd.yaml

Process Control Commands
------------------------

restart *PROCESS1* *PROCESS2*...

    restarts specified processes by name (sending it SIGTERM), temporarily
    disables failfast for this process

stop *PROCESS1* *PROCESS2*...

    stops process by sending it SIGTERM, disables failfast for this process

start *PROCESS1* *PROCESS2*...

    starts processes, even if they are were marked as stopped

sigterm *PROCESS1* *PROCESS2*...

sighup *PROCESS1* *PROCESS2*...

sigkill *PROCESS1* *PROCESS2*...

sigusr1 *PROCESS1* *PROCESS2*...

sigusr2 *PROCESS1* *PROCESS2*...

sigint *PROCESS1* *PROCESS2*...

sigquit *PROCESS1* *PROCESS2*...

    sends respective signal to a process (that will probably die, and
    kill bossrun in running in failfast mode, not like restart)

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

  -a -A    match (no match) command arguments

  -b -B    match (no match) executable path

  -p -P    match (no match) pid of running processes

  -n -N    match (no match) configured process name (on by default)

  -e -E    match by fnmatch pattern (exact match)

Bossrun Global Commands
-----------------------

shutdown

    shut down bossrun and all started processes

Environment
-----------

BOSS_CONFIG

    specifies bossrun configuration file instead of specifying it on command
    line. Very useful to test local configuration, also works for shell
    completion.
