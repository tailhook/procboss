#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from waflib import Utils, Options
from waflib.Build import BuildContext
from waflib.Scripting import Dist
import subprocess
import os.path

APPNAME='procboss'
if os.path.exists('.git'):
    VERSION=subprocess.getoutput('git describe').lstrip('v').replace('-', '_')
else:
    VERSION='0.2.3'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')
    conf.find_program('rst2man', var='RST2MAN')
    conf.find_program('gzip', var='GZIP')

def build(bld):
    import coyaml.waf
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'src/config.yaml',
            'src/runcommand.c',
            'src/bossrun.c',
            'src/bossruncmd.c',
            'src/entry.c',
            'src/log.c',
            'src/control.c',
            'src/procman.c',
            ],
        target       = 'bossrun',
        includes     = ['src'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['yaml', 'coyaml'],
        )
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'src/config.yaml',
            'src/runcommand.c',
            'src/log.c',
            'src/bossd.c',
            'src/bossdcmd.c',
            'src/entry.c',
            'src/control.c',
            'src/procman.c',
            'src/util.c',
            ],
        target       = 'bossd',
        includes     = ['src'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['yaml', 'coyaml'],
        )
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'linenoise/linenoise.c',
            'src/bossruncmd.c',
            'src/config.c',
            'src/shellutil.c',
            'src/bossrc.c',
            ],
        target       = 'bossrc',
        includes     = ['src', 'linenoise'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['yaml', 'coyaml'],
        defines      = [ 'NOACTIONS' ],
        )
    bld(
        features     = ['c', 'cprogram', 'coyaml'],
        source       = [
            'linenoise/linenoise.c',
            'src/bossdcmd.c',
            'src/config.c',
            'src/shellutil.c',
            'src/bossctl.c',
            ],
        target       = 'bossctl',
        includes     = ['src', 'linenoise'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['yaml', 'coyaml'],
        defines      = [ 'NOACTIONS' ],
        )
    bld(
        features     = ['c', 'cprogram'],
        source       = [
            'src/bosstree.c',
            'src/config.c',
            'src/util.c',
            ],
        target       = 'bosstree',
        includes     = ['src'],
        cflags       = ['-std=gnu99', '-Wall'],
        lib          = ['coyaml', 'yaml'],
        )
    bld(rule='${RST2MAN} ${SRC} ${TGT}',
        source='doc/bossd.rst', target='doc/bossd.8')
    bld(rule='${RST2MAN} ${SRC} ${TGT}',
        source='doc/bossctl.rst', target='doc/bossctl.8')
    bld(rule='${RST2MAN} ${SRC} ${TGT}',
        source='doc/bossrun.rst', target='doc/bossrun.1')
    bld(rule='${RST2MAN} ${SRC} ${TGT}',
        source='doc/bossrc.rst', target='doc/bossrc.1')
    bld(rule='${RST2MAN} ${SRC} ${TGT}',
        source='doc/bosstree.rst', target='doc/bosstree.1')
    bld(rule='${GZIP} -f ${SRC}',
        source='doc/bossd.8', target='doc/bossd.8.gz')
    bld(rule='${GZIP} -f ${SRC}',
        source='doc/bossctl.8', target='doc/bossctl.8.gz')
    bld(rule='${GZIP} -f ${SRC}',
        source='doc/bossrun.1', target='doc/bossrun.1.gz')
    bld(rule='${GZIP} -f ${SRC}',
        source='doc/bossrc.1', target='doc/bossrc.1.gz')
    bld(rule='${GZIP} -f ${SRC}',
        source='doc/bosstree.1', target='doc/bosstree.1.gz')
    bld.install_as('${PREFIX}/share/zsh/site-functions/_bossrc',
        'completion/zsh_bossrc')
    bld.install_as('${PREFIX}/share/zsh/site-functions/_bossctl',
        'completion/zsh_bossctl')
    if bld.env['PREFIX'] == '/usr':
        bld.install_as('/etc/bash_completion.d/procboss',
            'completion/bash')
    else:
        bld.install_as('${PREFIX}/etc/bash_completion.d/procboss',
            'completion/bash')
    bld.install_files('${PREFIX}/share/man/man8',
        ['doc/bossd.8.gz', 'doc/bossctl.8.gz'])
    bld.install_files('${PREFIX}/share/man/man1',
        ['doc/bossrun.1.gz', 'doc/bossrc.1.gz', 'doc/bosstree.1.gz'])


def dist(ctx):
    ctx.excl = [
        'doc/_build/**',
        '.waf*', '*.tar.gz', '*.zip', 'build',
        '.git*', '.lock*', '**/*.pyc', '**/*.swp', '**/*~',
        '.boss*',
        ]
    ctx.algo = 'tar.gz'


def make_pkgbuild(task):
    import hashlib
    task.outputs[0].write(Utils.subst_vars(task.inputs[0].read(), {
        'VERSION': VERSION,
        'DIST_MD5': hashlib.md5(task.inputs[1].read('rb')).hexdigest(),
        }))


def archpkg(ctx):
    from waflib import Options
    Options.commands = ['dist', 'makepkg'] + Options.commands


def build_package(bld):
    distfile = APPNAME + '-' + VERSION + '.tar.gz'
    bld(rule=make_pkgbuild,
        source=['PKGBUILD.tpl', distfile, 'wscript'],
        target='PKGBUILD')
    bld(rule='cp ${SRC} ${TGT}', source=distfile, target='.')
    bld.add_group()
    bld(rule='makepkg -f', source=distfile)
    bld.add_group()
    bld(rule='makepkg -f --source', source=distfile)


class makepkg(BuildContext):
    cmd = 'makepkg'
    fun = 'build_package'
    variant = 'archpkg'

