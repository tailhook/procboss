========
bosstree
========

-------------------------------
boss-controled processes viewer
-------------------------------

:Author: Paul Colomiets <paul@colomiets.name>
:Date: 2011
:Manual section: 1

Synopsis
--------

| bosstree [ -acdimpurtoHUN ] [-A | -C config.yaml | -P pid]

Description
-----------

Bosstree is an utility to view all processes started from within bossd and
bossrun supervisors, and their children

Options
-------

Tree selection options:

   -A       Show all processes started from any bossd
   -C FILE  Show only processed started from bossd with config FILE
   -P PID   Show only processes started from bossd with pid PID

Process selection options:

   -d       Show processes which detached from bossd
   -c       Show children of started processes

Display options:

   -H       Don't show hierarchy (useful for scripts)
   -a       Show command-line of each process
   -p       Show pid of each process
   -t       Show thread number of each process
   -U       Don't show uptime of the process
   -N       Don't show name of the process
   -i       Show instance number of the process
   -r       Show RSS(Resident Set Size) of each process
   -v       Show virtual memory size of each process
   -u       Show CPU usage of each process (in monitor mode)
   -o       Colorize output
   -O       Dont' colorize output
   -m IVL   Monitor (continuously display all the processes with
            interval IVL in seconds

