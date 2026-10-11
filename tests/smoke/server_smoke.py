#!/usr/bin/env python3
"""Dedicated server smoke test on the stub game directory.

Starts jk2mvded on maps/ci_box.bsp, fills the server with 31 bots, runs a
map_restart and a map reload, and checks that the server quits cleanly with
every bot still connected, and that the map_restart restarts the game without
reloading the map. This exercises the filesystem, the game module, botlib,
collision and snapshot building without any retail asset.

A second, short run starts with sv_maxclients 64 and checks that it ends at 32
on a 1.04 map. Today that is the cap of SV_BoundMaxClients, MAX_CLIENTS, which
runs before the version is chosen; the 32 clients of protocols 15 and 16 have
no cap of their own until step 2 of docs/PROTOCOL-1.05.md adds one after the
choice.

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


def crash_logs(work):
    """Crash logs where the engine writes them: next to the game for portable
    builds, in ~/.jk2mv for installed ones."""
    return set(glob.glob(os.path.join(work, 'crashlog-*.txt')) +
               glob.glob(os.path.join(os.path.expanduser('~'), '.jk2mv', 'crashlog-*.txt')))


def crash_log_pid(path):
    """The process a crash log is about, or None."""
    try:
        with open(path, 'r', errors='replace') as f:
            match = re.search(r'^Process ID:\s*(\d+)', f.read(), re.M)
    except OSError:
        return None
    return int(match.group(1)) if match else None


def own_crash_logs(paths, work, pids):
    """The new crash logs of these servers: ~/.jk2mv is shared with any other
    jk2mv running meanwhile (another test, a game), so there only the logs
    of these processes count."""
    return sorted(p for p in paths
                  if os.path.dirname(os.path.abspath(p)) == work or crash_log_pid(p) in pids)


def run_server(cmd, work, logname, timeout):
    """Run a dedicated server; return its exit code, its log without colors and
    timestamps, and its process ID."""
    proc = subprocess.Popen(cmd, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        out = proc.communicate(timeout=timeout)[0]
        rc = proc.returncode
    except subprocess.TimeoutExpired:
        proc.kill()
        rc, out = 'timeout', proc.communicate()[0]
    out = out.decode('latin-1')
    # drop color codes and the com_timestamps prefix of dedicated servers
    log = re.sub(r'\x1b\[[0-9;]*m', '', out)
    log = re.sub(r'^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d ', '', log, flags=re.M)
    with open(os.path.join(work, logname), 'w') as f:
        f.write(log)
    return rc, log, proc.pid


def clamp_errors(ded, work):
    """A second, short server: sv_maxclients 64 ends at 32, the cap of
    SV_BoundMaxClients (MAX_CLIENTS, before the version is chosen). Not in the
    main run: the clamp leaves sv_maxclients modified, and map_restart would
    then reload the map instead of restarting it. Returns the errors and the
    server's process ID."""
    with open(os.path.join(work, 'base', 'clamp.cfg'), 'w') as f:
        f.write('wait 20\nsv_maxclients\nquit\n')
    cmd = [ded, '+set', 'dedicated', '1', '+set', 'fs_basepath', work, '+set', 'fs_homepath', work,
           '+set', 'ttycon', '0', '+set', 'sv_hibernateFps', '0', '+set', 'sv_maxclients', '64',
           '+map', 'ci_box', '+exec', 'clamp.cfg']
    rc, log, pid = run_server(cmd, work, 'clamp.log', 120)
    errors = []
    if rc != 0:
        errors.append('clamp run: exit code %s' % rc)
    if '"sv_maxclients" is:"32' not in log:
        errors.append('clamp run: sv_maxclients 64 was not clamped to 32')
    return errors, pid


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
    # ~/.jk2mv keeps the crash logs of earlier sessions: only new ones count
    old_crashlogs = crash_logs(work)
    rc, log, pid = run_server(cmd, work, 'server.log', 300)

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
    if first >= 0 and second >= 0 and 'variable change -- restarting.' in log[first:second]:
        errors.append('map_restart reloaded the map instead of restarting it')
    clamp, clamp_pid = clamp_errors(ded, work)
    errors += clamp
    crashlogs = own_crash_logs(crash_logs(work) - old_crashlogs, work, {pid, clamp_pid})
    if crashlogs:
        errors.append('crash logs written: %s' % ', '.join(crashlogs))

    if errors:
        print('\n'.join(log.splitlines()[-60:]))
        print('server smoke test FAILED:\n  ' + '\n  '.join(errors))
        sys.exit(1)
    print('server smoke test passed: %d bots, map_restart and map reload, clean exit; sv_maxclients 64 clamped to 32' % NUM_BOTS)


if __name__ == '__main__':
    main()
