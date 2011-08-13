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

reopenlog

    reopen log files, in case some external tool removed or rotated it

rotatelog

    force log rotation
