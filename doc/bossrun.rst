=======
bossrun
=======

----------------------------
tiny supervisor-like utility
----------------------------

:Author: Paul Colomiets <paul@colomiets.name>
:Date: 2011
:Manual section: 1

Synopsis
--------

| bossrun [ -fFrR ] [ -c *CONFIG_FILE* ]

Description
-----------

Bossrun is an utility to run bossd config from unprivileged user without
detaching of console. Useful for debugging multi process systems.

Options
-------

  -c CONFIG_FILE
    configuration file. By default bossrun will try ./bossrun.yaml,
    ./boss.yaml, ./config/bossrun.yaml, ./config/boss.yaml and will fall back
    to /etc/bossd.yaml if neither exists.

  -F, --failfast
    stop all processes and bossrun itself when one of the processes dead

  -f, --no-fail
    do not stop all processes when one of the process dead

  -R, --restart
    restart dead processes (like in bossd), unless failfast is specified

  -r, --no-restart
    do not restart dead processes

Environment
-----------

BOSS_CONFIG

    specifies bossrun configuration file instead of specifying it on command
    line. Very useful to test local configuration, also works for shell
    completion.
