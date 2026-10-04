#!/usr/bin/env python3
"""Dedicated server smoke test on the stub game directory.

Starts jk2mvded on maps/ci_box.bsp, fills the server with 31 bots, runs a
map_restart and a map reload, and checks that the server quits cleanly with
every bot still connected. This exercises the filesystem, the game module,
botlib, collision and snapshot building without any retail asset.

usage: server_smoke.py <jk2mvded> <directory holding the built base/> <work dir>
"""

import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Com_Error banner, crash handler and network overflows
FATAL_MARKERS = ('********************\nERROR', 'Server crashed', 'recursive error', 'overflow')
NUM_BOTS = 31


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    ded, built, work = (os.path.abspath(a) for a in sys.argv[1:])
    subprocess.check_call([sys.executable, os.path.join(HERE, 'make_stub_base.py'), work, built])

    cfg = ['wait 20']
    for i in range(1, NUM_BOTS + 1):
        cfg += ['addbot B%02d 4' % i, 'wait 5']      # bots added at once get kicked for overflow
    # 'map_restart' alone waits 5 s and would run after 'quit': restart now
    cfg += ['wait 200', 'status', 'map_restart 0', 'wait 100', 'status', 'map ci_box', 'wait 200', 'status', 'quit']
    with open(os.path.join(work, 'base', 'smoke.cfg'), 'w') as f:
        f.write('\n'.join(cfg) + '\n')

    cmd = [ded, '+set', 'dedicated', '1', '+set', 'fs_basepath', work, '+set', 'fs_homepath', work,
           '+set', 'ttycon', '0', '+set', 'sv_hibernateFps', '0', '+set', 'sv_maxclients', '32',
           '+map', 'ci_box', '+exec', 'smoke.cfg']
    try:
        proc = subprocess.run(cmd, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
        rc, out = proc.returncode, proc.stdout.decode('latin-1')
    except subprocess.TimeoutExpired as e:
        rc, out = 'timeout', (e.stdout or b'').decode('latin-1')
    # drop color codes and the com_timestamps prefix of dedicated servers
    log = re.sub(r'\x1b\[[0-9;]*m', '', out)
    log = re.sub(r'^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d ', '', log, flags=re.M)
    with open(os.path.join(work, 'server.log'), 'w') as f:
        f.write(log)

    errors = []
    if rc != 0:
        errors.append('exit code %s%s' % (rc, ' (signal %d)' % -rc if isinstance(rc, int) and rc < 0 else ''))
    for marker in FATAL_MARKERS:
        if marker in log:
            errors.append('log contains %r' % marker)
    statuses = log.split('num score ping name')[1:]
    if len(statuses) != 3:
        errors.append('expected three status outputs, got %d' % len(statuses))
    else:
        for when, status in zip(('after joining', 'after the map_restart', 'after the map reload'), statuses):
            bots = len(re.findall(r'^\s*\d+\s+-?\d+\s+\d+\s+.*\sbot\s', status, re.M))
            if bots != NUM_BOTS:
                errors.append('%d bots connected %s, expected %d' % (bots, when, NUM_BOTS))
    # the restart must really happen between the first two statuses
    first, second = log.find('num score ping name'), log.find('num score ping name', log.find('num score ping name') + 1)
    if first >= 0 and second >= 0 and 'ShutdownGame' not in log[first:second]:
        errors.append('map_restart did not restart the game')
    # portable builds write crash logs next to the game, installed ones in ~/.jk2mv
    crashlogs = (glob.glob(os.path.join(work, 'crashlog-*.txt')) +
                 glob.glob(os.path.join(os.path.expanduser('~'), '.jk2mv', 'crashlog-*.txt')))
    if crashlogs:
        errors.append('crash logs written: %s' % ', '.join(crashlogs))

    if errors:
        print('\n'.join(log.splitlines()[-60:]))
        print('server smoke test FAILED:\n  ' + '\n  '.join(errors))
        sys.exit(1)
    print('server smoke test passed: %d bots, map_restart and map reload, clean exit' % NUM_BOTS)


if __name__ == '__main__':
    main()
