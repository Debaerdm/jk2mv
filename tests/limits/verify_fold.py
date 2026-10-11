#!/usr/bin/env python3
"""Check that the limit accessors of src/qcommon/mv_limits.h fold back to the
constants they replaced (docs/PROTOCOL-1.05.md, step 1).

Every translation unit of jk2mvmp and jk2mvded is compiled twice with the
build's own command line: the source of a base revision, and the current
source with -DMV_LIMITS_VERIFY_FOLD, where MV_MaxClients() and
MV_MaxGEntities() are MAX_CLIENTS and MAX_GENTITIES again. The two objects
must have the same sections and sizes (.bss included), the same instructions
and initialised data, and the same relocations (which function a call goes to,
which global an access reads, where a pointer in the data points). The symbol
table and the debug information are not compared. The units of EXPECTED may
differ: their code changes on purpose, with the same behaviour.

Linux and GNU binutils only; configure a Release build with
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON first.

usage: verify_fold.py <build dir> <base revision | base source tree> [jobs]
"""

import concurrent.futures
import io
import json
import os
import re
import shlex
import subprocess
import sys
import tarfile
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TARGETS = ('/jk2mvmp.dir/', '/jk2mvded.dir/')
# the botlib's "maxentities" default is va("%i", MV_MaxGEntities()) instead of
# the string "1024": a call instead of a constant, the same value
EXPECTED = {'src/botlib/be_aas_main.cpp', 'src/botlib/be_interface.cpp'}
# __DATE__ and __TIME__ (sys_unix.cpp, common.cpp) the same in both compiles
ENV = dict(os.environ, SOURCE_DATE_EPOCH='0')


def units(build):
    with open(os.path.join(build, 'compile_commands.json')) as f:
        entries = json.load(f)
    src = os.path.join(REPO, 'src') + os.sep
    for e in entries:
        args = e['arguments'] if 'arguments' in e else shlex.split(e['command'])
        out = args[args.index('-o') + 1]
        path = os.path.normpath(os.path.join(e['directory'], e['file']))
        if any(t in out for t in TARGETS) and path.startswith(src) and \
                not path.startswith(os.path.join(src, 'mvsdk') + os.sep):
            yield e['directory'], args, os.path.relpath(path, REPO).replace(os.sep, '/'), out


def base_tree(base, tmp):
    if os.path.isdir(base):
        return os.path.abspath(base)
    tar = subprocess.run(['git', '-C', REPO, 'archive', '--format=tar', base, 'src'],
                         check=True, stdout=subprocess.PIPE).stdout
    root = os.path.join(tmp, 'base')
    with tarfile.open(fileobj=io.BytesIO(tar)) as t:
        # filter='data' where tarfile has it (Python 3.12, and security backports)
        if hasattr(tarfile, 'data_filter'):
            t.extractall(root, filter='data')
        else:
            t.extractall(root)
    return root


def command(args, out, root, extra):
    """The build's command for this unit, writing out, with the sources of root."""
    cmd, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ('-MF', '-MT', '-MQ'):
            skip = True
            continue
        if a in ('-MD', '-MMD'):
            continue
        cmd.append(a.replace(os.path.join(REPO, 'src'), os.path.join(root, 'src')))
    cmd[cmd.index('-o') + 1] = out
    return cmd + extra


def objdump(obj, *args):
    return subprocess.run(['objdump'] + list(args) + [obj], check=True,
                          stdout=subprocess.PIPE, universal_newlines=True).stdout


def sections(obj):
    """The object as text without its path: the section table (names, sizes and
    flags, .bss included), the code disassembled with its relocations, and the
    data with the relocations into it. Without the relocations, a call to
    another function, a use of another global or a pointer to another object
    would look the same: their targets are zeros in the object."""
    # name, size, alignment and flags; not the file offsets, which follow the
    # debug information (line numbers) placed before some of the sections
    rows = [r for r in re.findall(r'^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+[0-9a-f]+\s+(\S+)\s+(.*)$',
                                  objdump(obj, '-h', '-w'), re.M)
            if not r[0].startswith(('.debug', '.zdebug'))]
    if not rows:
        sys.exit('cannot read the section table of %s' % obj)
    data = sum([['-j', r[0]] for r in rows if re.match(r'\.(rodata|data)(\.|$)', r[0])], [])
    text = ''.join('%s %s %s %s\n' % r for r in rows) + objdump(obj, '-d', '-r', '--no-show-raw-insn')
    if data:
        text += objdump(obj, '-s', *data) + objdump(obj, '-r', *data)
    return text.replace(obj, 'OBJ')


def check(job):
    directory, args, unit, out, base, tmp, n = job
    objs = []
    for name, root, extra in (('base', base, ['-ffile-prefix-map=%s=%s' % (base, REPO)]),
                              ('fold', REPO, ['-DMV_LIMITS_VERIFY_FOLD'])):
        obj = os.path.join(tmp, '%s-%d.o' % (name, n))
        r = subprocess.run(command(args, obj, root, extra), cwd=directory, env=ENV,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        if r.returncode:
            return unit, out, 'does not compile (%s):\n%s' % (name, r.stdout[-2000:])
        objs.append(obj)
    same = sections(objs[0]) == sections(objs[1])
    for o in objs:
        os.remove(o)
    return unit, out, None if same else 'differs'


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    build, base = os.path.abspath(sys.argv[1]), sys.argv[2]
    jobs = int(sys.argv[3]) if len(sys.argv) == 4 else 2
    with tempfile.TemporaryDirectory() as tmp:
        root = base_tree(base, tmp)
        todo = [(d, a, u, o, root, tmp, n) for n, (d, a, u, o) in enumerate(units(build))]
        if not todo:
            sys.exit('no jk2mvmp or jk2mvded unit in %s/compile_commands.json' % build)
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            results = list(pool.map(check, todo))
    errors, expected = [], []
    for unit, out, problem in results:
        if problem == 'differs' and unit in EXPECTED:
            expected.append(out)
        elif problem:
            errors.append('%s (%s): %s' % (unit, out, problem))
    if errors:
        print('\n'.join(errors))
        sys.exit('fold check FAILED')
    print('fold check passed: %d objects the same as the base, %d differ as expected:%s' %
          (len(results) - len(expected), len(expected), ''.join('\n  ' + o for o in sorted(expected))))


if __name__ == '__main__':
    main()
