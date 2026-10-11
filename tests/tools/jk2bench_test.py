#!/usr/bin/env python3
"""tools/bench/jk2bench.ps1 on a fake game, from paths with brackets.

The fake game (a shell script) writes a timedemo result into the
qconsole.log of the fs_homepath it is given. With -HomePath './h[1]', which
holds the demo, the run must write its CSV row and leave nothing but h[1]
and the CSV; with a home folder that doesn't exist yet, './n[2]', the script
must create that folder (no wildcard escapes in its name) and stop because
the demo isn't there.

usage: jk2bench_test.py <pwsh> <jk2bench.ps1> <work dir>
"""

import os
import shutil
import stat
import subprocess
import sys

FAKE_GAME = '''#!/bin/sh
home=""
while [ $# -gt 0 ]; do
  if [ "$1" = "+set" ] && [ "$2" = "fs_homepath" ]; then home="$3"; fi
  shift
done
mkdir -p "$home/base"
echo "1000 frames, 2.5 seconds: 400.0 fps" > "$home/base/qconsole.log"
'''


def run_bench(pwsh, script, run_dir, home):
    cmd = [pwsh, '-NoProfile', '-NonInteractive', '-File', script, '-Exe', '../bin[1]/jk2mvmp',
           '-HomePath', home, '-Demos', 'd1', '-Presets', 'cpu', '-Runs', '1', '-Csv', 'out.csv',
           '-TimeoutSec', '60']
    proc = subprocess.run(cmd, cwd=run_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300,
                          env=dict(os.environ, NO_COLOR='1', TERM='dumb'))
    return proc.returncode, proc.stdout.decode('utf-8', 'replace')


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    pwsh, script, work = sys.argv[1], os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3])
    shutil.rmtree(work, ignore_errors=True)
    game = os.path.join(work, 'bin[1]', 'jk2mvmp')
    os.makedirs(os.path.dirname(game))
    with open(game, 'w') as f:
        f.write(FAKE_GAME)
    os.chmod(game, os.stat(game).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    errors = []
    # the home folder exists and holds the demo
    run_dir = os.path.join(work, 'existing')
    os.makedirs(os.path.join(run_dir, 'h[1]', 'base', 'demos'))
    open(os.path.join(run_dir, 'h[1]', 'base', 'demos', 'd1.dm_16'), 'w').close()
    rc, out = run_bench(pwsh, script, run_dir, './h[1]')
    if rc != 0:
        errors.append('existing home: exit code %d\n%s' % (rc, out))
    csv = os.path.join(run_dir, 'out.csv')
    rows = open(csv).read().split() if os.path.isfile(csv) else []
    if rows[1:] != ['cpu,d1,1,1000,2.500,400.0']:
        errors.append('existing home: CSV rows %r' % rows)
    if sorted(os.listdir(run_dir)) != ['h[1]', 'out.csv']:
        errors.append('existing home: left %r' % sorted(os.listdir(run_dir)))

    # the home folder doesn't exist: created as named, then no demo in it
    run_dir = os.path.join(work, 'missing')
    os.makedirs(run_dir)
    rc, out = run_bench(pwsh, script, run_dir, './n[2]')
    if rc == 0 or 'no demo' not in out:
        errors.append('missing home: expected the "no demo" error, got exit code %d\n%s' % (rc, out))
    if sorted(os.listdir(run_dir)) != ['n[2]']:
        errors.append('missing home: left %r' % sorted(os.listdir(run_dir)))

    if errors:
        print('jk2bench.ps1 test FAILED:\n  ' + '\n  '.join(errors))
        sys.exit(1)
    print('jk2bench.ps1 test passed: bracketed paths taken literally')


if __name__ == '__main__':
    main()
