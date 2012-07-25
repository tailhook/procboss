Process Boss
============

Process boss is a tiny supervisor program.

Features:

* Running programs and restarting on crash
* Setting environment, uid, chroot, scheduling, affinity without running tons
  of external utilities
* Opening files and providing them as file descriptors (useful to bind to
  tcp port < 1024 or for fastcgi programs)
* Fast restart in case of crash (reaping SIGCHLD)
* Control interface throught fifo to stop/restart processes
* User-friendly and featureful configuration in yaml
* Bash and Zsh completion for remote control utilities
* Logging of process deaths and restarts
* Running multiple similar processes, optionally keeping some shared file
  descriptor open. Dynamically adding processed.
* In case it's not clear from above, can act as spawn-fcgi
* Tagging processed for easier managing, like restart all processes of some
  project

Will never happen:

* Monitoring memory, cpu, whatever (there are better tools, e.g. collectd)
* Being replacement for init
* Networking
* Other bloat

Bugs:

We currently use environment variables for all the process tracking, which
have the following implications:

* it doesn't work for some programs trying to mangle own environment like
  nginx (althrought nginx has it's own supervisor)
* it's not totally secure, any bossd child can imitate other child, by modifing
  environment, blocking that one to restart in case of crash (this
  vulnerability is tradeoff of being very robust, and it's quite hard to
  exploit for our setup, but you have been warned!)

Dependencies
------------

Runtime dependencies:

    * linux kernel 2.6.27 (needs signalfd)
    * libyaml

Build dependencies:

    * python3
    * coyaml
    * docutils (for python3)

Building
--------

Basic workflow is the following::

    git clone git://github.com/tailhook/procboss.git
    cd procboss
    git submodule update --init
    ./waf configure --prefix=/usr
    ./waf build
    sudo ./waf install
