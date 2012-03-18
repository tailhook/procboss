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

Configuration file is normal YAML_ file. It has three main sections::

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

TBD

Bossd
-----

TBD

Bossctl
-------

No options particularly interesting here. To see all possible options with
their help string use::

    bossctl -PP


Processes
---------

TBD

.. _YAML: http://yaml.org
