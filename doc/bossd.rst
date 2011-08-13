=====
bossd
=====

-------------------------
process supervisor daemon
-------------------------

:Author: Paul Colomiets <paul@colomiets.name>
:Date: 2011
:Manual section: 8

Synopsis
--------

| bossd [ -c /config/path.yaml ]
| bossd [ -c /config/path.yaml ] -C [ -P ]

Description
-----------

Bossd is usually run from init without arguments. It starts processes from
configuration file. It watchs and restarts dead processes.

Options
-------

  -c CONFIG_FILE

    configuration file. Default is /etc/bossd.yaml

  -C  check configuration and exit

  -P  print loaded configuration file (can be useful to debug errors in
      configuration)
