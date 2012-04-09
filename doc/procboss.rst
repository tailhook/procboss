=====
bossd
=====

-------------------------
process supervisor daemon
-------------------------

:Author: Paul Colomiets <paul@colomiets.name>
:Date: 2011
:Manual section: 5

Synopsis
--------

Default configuration file for ``bossd`` and ``bossctl`` utilities is:

    /etc/bossd.yaml

Default configuration for ``bossrun`` and ``bossrc`` utilities is searched
in the current folder, with the following names (in order of priority):

    ./bossrun.yaml
    ./boss.yaml
    ./config/bossrun.yaml
    ./config/boss.yaml

Each utility (incl. ``bossd``, ``bossctl``, ``bossrun``, ``bossrc``) supports
the following options:

  -c,--config CONFIG_FILE.yaml
    alternate configuration file

  -C,--check-config
    check config and exit

  -P,--pring-config
    print configuration as it is parsed, including expansion of all variables
    and command-line options. Specify this option twice to see configuration
    file with comments

  -D,--config-var NAME=VALUE
    define a variable which can be used inside configuration file (see below)

  --debug-config
    print debugging information for configuration file parser (use with -C).
    This log should be attached to bug report for easier debugging.

  --config-vars,--config-no-vars
    enable/disable configuration variables parser, useful if you have many
    dollars in configuration file, and don't want variables

  --config-show-variables
    show variables used in configuration file along with their values

Description
-----------

Configuration file is normal YAML_ file. It has four main sections::

    bossrun:
      # configuration of bossrun process supervisor
    bossd:
      # configuration of bossd process supervisor
    bossctl:
      # default configuration of bossctl and bossrc utilities
    Processes:
      # processes to run by bossrun or bossd supervisors

Each section will be described in details further.

TBD: describe basic rules

Bossrun
-------

This section is only used when using ``bossrun`` binary. Two useful options
here:

failfast: yes|no
    Stop all processes and shutdown if one of the processes is dead (unless
    this particular process is scheduled for restart by manual command)

restart: yes|no
    Restart dead processes (only useful if failfast is set to ``yes``)

.. note::

   bossrun is used for debugging purposes, so it's useful to see dead processes
   in the hard way. Bossd has no failfast or no restart mode.


Bossd
-----

This section is only used when using ``bossd`` binary.

fifo: PATH
    Where to put control fifo. Fifo is used by ``bossctl`` to communicate with
    bossd instance. Please don't use a word-writable directory as its insecure

pid-file: PATH
    Path to file which contains pid of bossd process. Pid file is not used
    by boss, but can be used for other utilities

logging:
    Logging subsection controls logging of bossd

    file: PATH
        Path to logfile

    mode: 0644
        Mode of logfile, use 0-prefixed digits to have convenient octal
        notation

    rotation-size: 10Mi
        Maximum size of single log file, before it rotates. Use suffix as in
        example to avoid writing long numbers in bytes

    rotation-time: 604800
        Time in seconds of maximum time log can be written to without rotation

    rotation-backups: 9
        Number of backups to keep after rotating file

timeouts:
    Time constraints for process restart. The following are default values::

        successful-run: 10
        small-restart: 0.1
        retries: 2
        big-restart: 120

    If process dies, bossd tries to restart it after a ``small-restart``
    timeout (in seconds). If process dies again for ``retries`` number of
    times, then bossd switches to a ``big-restart`` timeout. And as you
    might guessed, process is considered to be started successfully if has been
    run for at least ``successful-run`` seconds.


Bossctl
-------

No options particularly interesting here. To see all possible options with
their help string use::

    bossctl -PP


Processes
---------

TBD

.. _YAML: http://yaml.org
