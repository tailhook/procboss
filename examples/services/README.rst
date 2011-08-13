Bossd Service Configuration Examples
------------------------------------

This directory is usually installed as ``/etc/boss.d`` and is
included on by service basis in ``/etc/boss.yaml``, like
following::

    Processes:
      mysql: !Include boss.d/mysql.yaml

Most paths are set up as appropriate for ArchLinux, so you
probably need to tweak them. Some of services also include
customization variables. You can set the either in boss.yaml or in
any included file like following::

    _tweaks:
      - &mysql_data_dir /data/mysql

Note: undersored variables are not parsed as configuration keys, but can be used for anchors which are used later
