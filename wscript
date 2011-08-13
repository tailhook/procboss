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
    VERSION='0.2'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')

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
        cflags       = ['-std=c99', '-Wall'],
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
            ],
        target       = 'bossd',
        includes     = ['src'],
        cflags       = ['-std=c99', '-Wall'],
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
        cflags       = ['-std=c99', '-Wall'],
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
        cflags       = ['-std=c99', '-Wall'],
        lib          = ['yaml', 'coyaml'],
        defines      = [ 'NOACTIONS' ],
        )
    bld(
        features     = ['c', 'cprogram'],
        source       = [
            'src/bosstree.c',
            ],
        target       = 'bosstree',
        includes     = ['src'],
        cflags       = ['-std=c99', '-Wall'],
        lib          = [],
        )
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


def dist(ctx):
    ctx.excl = [
        'doc/_build/**',
        '.waf*', '*.tar.gz', '*.zip', 'build',
        '.git*', '.lock*', '**/*.pyc', '**/*.swp', '**/*~'
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

