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

