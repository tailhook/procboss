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
