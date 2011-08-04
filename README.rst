Process Boss
============

Process boss is a tiny supervisor program.

Features:

* Running programs and restarting on crash
* Setting environment, uid, chroot without running tons of external utilities
* Opening files and providing them as file descriptors (useful to bind to
  tcp port < 1024 or for fastcgi programs)
* Fast restart in case of crash (reaping SIGCHLD)
* Control interface throught fifo to stop/restart processes
* User-friendly and featureful configuration in yaml

To Do:

* Running multiple similar processes (like in fastcgi), optionally keeping
  some shared file descriptor open
* Configuration reloading (partially works, by restarting in-place)
* Better report of processes status
* Good logging of process shutdowns

Will never happen:

* Monitoring memory, cpu, whatever (there are better tools, e.g. collectd)
* Being replacement for init
* Networking
* Other bloat

