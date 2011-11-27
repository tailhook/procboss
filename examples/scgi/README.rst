SCGI Startup Example
====================

This is basically example of SCGI application with multiple processes. FastCGI
work in similar way, and SCGI is chosen for being able to run test without any
python dependencies.

Dependencies
------------

* python (>= python2.7, inc. python3.x)
* lighttpd

Running
-------

To test it run::

    bossrun -Rf -c boss.yaml

Where ``-R`` means restart if something fails, and ``-f`` means do not shutdown
if some process fails. We could put those options in config, but we tryed to
keep configuration file as short as possible. ``-c boss.yaml`` reads
configuration from specified file.

``bossrun`` is mainly useful for running in test environment (that's why ``-R``
is not default, you want to know when your process crashed). To run seriously
you may want to run bossd::

    bossd -c boss.yaml

But before you need to setup log, pid, and fifo directories, and probably setup
full paths for all the parts of the application. Also it's nice to run bossd as
root, and to run it from inittab or some other startup script.

