#!/usr/bin/env python3
"""Headless client smoke test on the stub game directory.

Runs jk2mvmp with SDL's offscreen video driver and software OpenGL (Mesa
llvmpipe in CI), opens the console, takes a screenshot, does a vid_restart
and takes another one. This covers renderer start-up, extension detection,
the bundled shaders and fonts, the menu module, the post-process path and
screenshot output. It can't enter a map: cgame needs the retail player
models.

usage: client_smoke.py <jk2mvmp> <directory holding the built base/> <work dir>
"""

import glob
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Com_Error banner; plain 'ERROR:' lines also come from harmless menu parse warnings
FATAL_MARKERS = ('********************\nERROR', 'recursive error', 'Segmentation fault')
SHOTS = ('ci_console', 'ci_after_vidrestart')   # captured at the end of the next frame


def read_tga(path):
    with open(path, 'rb') as f:
        data = f.read()
    id_len, cmap_type, img_type = data[0], data[1], data[2]
    width, height, bpp = struct.unpack('<HHB', data[12:17])
    if cmap_type != 0 or img_type != 2 or bpp not in (24, 32):
        raise ValueError('%s: unexpected TGA type %d/%d bpp' % (path, img_type, bpp))
    start = 18 + id_len
    pixels = data[start:start + width * height * (bpp // 8)]
    return width, height, bpp, pixels


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    client, built, work = (os.path.abspath(a) for a in sys.argv[1:])
    subprocess.check_call([sys.executable, os.path.join(HERE, 'make_stub_base.py'), work, built])
    # native menu module next to the game directory
    for lib in glob.glob(os.path.join(built, 'jk2mvmenu_*')):
        shutil.copy(lib, work)

    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1')
    cmd = [client, '+set', 'fs_basepath', work, '+set', 'fs_homepath', work,
           '+set', 'r_allowsoftwaregl', '1', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '3',
           '+set', 's_initsound', '0', '+set', 'com_introplayed', '1',
           '+wait', '60', '+toggleconsole', '+wait', '30', '+screenshot_tga', SHOTS[0], '+wait', '5',
           '+vid_restart', '+wait', '30', '+screenshot_tga', SHOTS[1], '+wait', '5', '+quit']
    try:
        proc = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=180)
        rc, out = proc.returncode, proc.stdout.decode('latin-1')
    except subprocess.TimeoutExpired as e:
        rc, out = 'timeout', (e.stdout or b'').decode('latin-1')
    # drop color codes and the com_timestamps prefix of dedicated servers
    log = re.sub(r'\x1b\[[0-9;]*m', '', out)
    log = re.sub(r'^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d ', '', log, flags=re.M)
    with open(os.path.join(work, 'client.log'), 'w') as f:
        f.write(log)

    errors = []
    if rc != 0:
        errors.append('exit code %s' % rc)
    for marker in FATAL_MARKERS:
        if marker in log:
            errors.append('log contains %r' % marker)
    renderer = re.search(r'GL_RENDERER: (.*)', log)
    print('GL_RENDERER:', renderer.group(1) if renderer else 'not found')
    for name in SHOTS:
        path = os.path.join(work, 'base', 'screenshots', name + '.tga')
        if not os.path.isfile(path):
            errors.append('missing screenshot %s' % path)
            continue
        try:
            width, height, bpp, pixels = read_tga(path)
        except ValueError as e:
            errors.append(str(e))
            continue
        step = bpp // 8
        colors = {pixels[i:i + 3] for i in range(0, len(pixels), step * 97)}
        if (width, height) != (640, 480):
            errors.append('%s is %dx%d, expected 640x480' % (name, width, height))
        if len(colors) < 3:
            errors.append('%s looks blank (%d distinct colors sampled)' % (name, len(colors)))

    if errors:
        print('\n'.join(log.splitlines()[-60:]))
        print('client smoke test FAILED:\n  ' + '\n  '.join(errors))
        sys.exit(1)
    print('client smoke test passed: renderer up, console drawn, vid_restart, screenshots written')


if __name__ == '__main__':
    main()
