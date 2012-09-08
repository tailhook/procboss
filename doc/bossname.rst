========
bossname
========

----------------------
process naming library
----------------------

:Author: Paul Colomiets <paul@colomiets.name>
:Date: 2012
:Manual section: 5

Synopsis
--------

On the command-line (for bash)::

    LD_PRELOAD=libbossname.so BOSSNAME_OVERRIDE=newname filename [args..]

On the command-line (other shells)::

    env LD_PRELOAD=libbossname.so BOSSNAME_OVERRIDE=newname filename [args..]

In the bossd config::

    Processes:
        some-django-site:
            executable-path: python
            arguments: [python, manage.py, runserver]
            workdir: /projects/django
            environment:
                LD_PRELOAD=libbossname.so
                BOSSNAME_OVERRIDE=some-django-site  # optional

Description
-----------

When building complex services with interpreted language, there are often
multiple processes with running same binary (e.g. python or ruby). There are a
lot of tools which can't differentiate between them (e.g. collectd) and some of
them don't do that conveniently (top, pstree).

libbossname is a tiny library which sets the name of process to the name of
boss config entry. You can also override the name in the environment. With the
override clause you may use the library without running boss.

The name of the process is one that is contained in ``/proc/<PID>/comm`` and
``/proc/<PID>/stat``. It is displayed in various utilities like ``top`` and
``pstree`` by default.

*Note:* To see the same name in ``ps`` you should put the name in the first
element of the ``arguments`` list. It's usually harmless, but some
binaries may check that element and act based on the name (argv[0])

After process renamed, libbossname does the following cleanup of the
environment variables:

* ``BOSS_CHILD`` is unset (internal variable tracking for boss)
* ``BOSSNAME_OVERRIDE`` is unset
* ``bossname.so`` (with any path) is removed from ``LD_PRELOAD``

In the case your process will exec during it's lifetime, you should set
``BOSSNAME_KEEPVARS`` so that environment vars will not be set, and are
usually inherited by the following ``exec()`` calls.



