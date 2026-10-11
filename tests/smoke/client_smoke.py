#!/usr/bin/env python3
"""Headless client smoke test on the stub game directory.

Runs jk2mvmp with SDL's offscreen video driver and software OpenGL (Mesa
llvmpipe in CI), opens the console, takes a screenshot, does a vid_restart
and takes another one. This covers renderer start-up, extension detection,
the bundled shaders and fonts, the menu module, the post-process path and
screenshot output. It can't enter a map: cgame needs the retail player
models. Then 'preset ultra' and 'preset classic' must only touch
registered cvars, and classic must undo exactly what ultra changed. Last, a
vid_restart while a local map load has the renderer down (the client
waits for the gamestate): the console drawn after it must not be white.

A motion JPEG video of the test scene (cl_aviMotionJpeg 1, the default)
must hold one JPEG frame of the render size per video frame, and leave no
frame sized buffer in the zone once stopped. Another one
starts under a console so full of text that the frame's render command
list has no room left to read the frame: those frames are dropped (empty
chunks), a JPEG screenshot taken then says it couldn't be, and once the
console closes the recording goes on.

With fail_malloc (a preloaded library, glibc only), a second run makes the
frame sized allocations fail, as in a 32 bit process: every screenshot and
the video must give up with a message, and the game go on. A third one, at
642x480, only fails the renderer's video capture buffer (the client's
frame buffer, of 4 byte aligned rows, is bigger): the motion JPEG video
starts, then ends at its first frame with a message.

With gl_context_check (preloaded, it reports GL calls without a current
context), a run where no window can be made (SDL's dummy driver) must end
with the fatal error and no GL call on the way down.

usage: client_smoke.py <jk2mvmp> <directory holding the built base/> <work dir>
                       [<fail_malloc.so> [<gl_context_check.so>]]
"""

import glob
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Com_Error banner; plain 'ERROR:' lines also come from harmless menu parse warnings.
# A crash shows as a negative exit code (the signal), see exit_error.
FATAL_MARKERS = ('********************\nERROR', 'recursive error')
SHOTS = ('ci_console', 'ci_after_vidrestart', 'ci_vidrestart_maploading')   # captured at the end of the next frame


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


def preset_lines(log, name):
    """What 'preset <name>' printed: {cvar: (old, new)} of its changes, and
    its other lines (a cvar it didn't find registered: a typo in the preset
    tables), or None without the preset."""
    if 'preset %s:\n' % name not in log:
        return None
    changes, others = {}, []
    for line in log.split('preset %s:\n' % name, 1)[1].splitlines():
        if not line.startswith('  '):
            break
        change = re.match(r'  (\S+): (.*) -> (.*)$', line)
        if change:
            changes[change.group(1)] = change.group(2, 3)
        else:
            others.append(line.strip())
    return changes, others


def exit_error(rc):
    """Description of a bad exit code, or None."""
    if rc == 0:
        return None
    if isinstance(rc, int) and rc < 0:
        return 'killed by signal %d' % -rc
    return 'exit code %s' % rc


def run_client(client, work, args, env=None):
    """Runs the client on the stub game directory: exit code and log. args
    come after the defaults and can override them."""
    env = dict(dict(os.environ, SDL_VIDEODRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1'), **(env or {}))
    cmd = [client, '+set', 'fs_basepath', work, '+set', 'fs_homepath', work,
           '+set', 'r_allowsoftwaregl', '1', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '3',
           '+set', 's_initsound', '0', '+set', 'com_introplayed', '1'] + args
    try:
        proc = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=180)
        rc, out = proc.returncode, proc.stdout.decode('latin-1')
    except subprocess.TimeoutExpired as e:
        rc, out = 'timeout', (e.stdout or b'').decode('latin-1')
    # drop color codes and the com_timestamps prefix of the console output
    log = re.sub(r'\x1b\[[0-9;]*m', '', out)
    log = re.sub(r'^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d ', '', log, flags=re.M)
    return rc, log


# 640x480 frames of 3 bytes per pixel, and the zone's header and tail
FRAME_BYTES = 640 * 480 * 3
CAPTURES = [('screenshot', 'ci_nomem', 'jpg'), ('screenshot_png', 'ci_nomem', 'png'),
            ('screenshot_tga', 'ci_nomem', 'tga')]


def check_capture_memory(client, work, fail_malloc):
    """Errors of the run where the frame sized allocations fail."""
    args = ['+wait', '30']
    for command, name, _ in CAPTURES:
        args += ['+' + command, name, '+wait', '5']     # taken at the end of the next frame
    args += ['+set', 'cl_aviMotionJpeg', '1', '+testscene', 'ci_box', '-256', '0', '96',
             '+wait', '5', '+video', 'ci_nomem', '+wait', '10', '+stopvideo', '+wait', '5', '+quit']
    rc, log = run_client(client, work, args, {'LD_PRELOAD': fail_malloc, 'FAIL_MALLOC_MIN': str(FRAME_BYTES),
                                               'FAIL_MALLOC_MAX': str(FRAME_BYTES + 256)})
    with open(os.path.join(work, 'client_nomem.log'), 'w') as f:
        f.write(log)
    errors = []
    if exit_error(rc):
        errors.append('without capture memory: ' + exit_error(rc))
    for marker in FATAL_MARKERS:
        if marker in log:
            errors.append('without capture memory: log contains %r' % marker)
    if log.count('not enough memory to capture a 640x480 frame') != len(CAPTURES):
        errors.append('without capture memory: %d screenshot warnings, expected %d'
                      % (log.count('not enough memory to capture a 640x480 frame'), len(CAPTURES)))
    if 'video: not enough memory for 640x480 frames' not in log:
        errors.append('without capture memory: the video started')
    for _, name, ext in CAPTURES:
        if os.path.exists(os.path.join(work, 'base', 'screenshots', '%s.%s' % (name, ext))):
            errors.append('without capture memory: %s.%s written' % (name, ext))
    if os.path.exists(os.path.join(work, 'base', 'videos', 'ci_nomem.avi')):
        errors.append('without capture memory: ci_nomem.avi written')
    return errors


def read_avi(path):
    """(width, height, frames of the AVI header, data of each video chunk)."""
    with open(path, 'rb') as f:
        data = f.read()
    avih = data.find(b'avih')
    total = struct.unpack('<I', data[avih + 24:avih + 28])[0]
    width, height = struct.unpack('<2I', data[avih + 40:avih + 48])
    chunks = []
    pos = data.find(b'movi') + 4
    while pos + 8 <= len(data):
        chunk, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        if chunk == b'idx1':
            break
        if chunk == b'00dc':
            chunks.append(data[pos + 8:pos + 8 + size])
        pos += 8 + size + (size & 1)
    return width, height, total, chunks


def jpeg_size(data):
    """(width, height) of a baseline JPEG image, None if it isn't one."""
    if not data.startswith(b'\xff\xd8') or b'\xff\xd9' not in data[-16:]:
        return None
    sof = data.find(b'\xff\xc0')
    if sof < 0:
        return None
    height, width = struct.unpack('>2H', data[sof + 5:sof + 9])
    return width, height


# the video starts on the test scene, a frame per engine frame
VIDEO_SCENE = ['+set', 'cl_aviMotionJpeg', '1', '+testscene', 'ci_box', '-256', '0', '96', '+wait', '5']


def check_motion_jpeg(client, work):
    """Errors of a motion JPEG recording of the test scene."""
    args = VIDEO_SCENE + ['+zone_stats', '+video', 'ci_mjpeg', '+wait', '20', '+stopvideo', '+wait', '5',
                          '+zone_stats', '+quit']
    rc, log = run_client(client, work, args)
    with open(os.path.join(work, 'client_mjpeg.log'), 'w') as f:
        f.write(log)
    errors = []
    if exit_error(rc):
        errors.append('motion JPEG video: ' + exit_error(rc))
    for marker in FATAL_MARKERS:
        if marker in log:
            errors.append('motion JPEG video: log contains %r' % marker)
    # the capture buffers of the client and the renderer are freed with the
    # recording, not kept until the next map load or vid_restart
    zone = [int(size) for size in re.findall(r'The zone is using (\d+) bytes', log)]
    if len(zone) != 2:
        errors.append('motion JPEG video: %d zone_stats, expected 2' % len(zone))
    else:
        print('motion JPEG video: the zone holds %d bytes before, %d after' % tuple(zone))
        if zone[1] - zone[0] >= FRAME_BYTES // 2:
            errors.append('motion JPEG video: %d more bytes in the zone after it' % (zone[1] - zone[0]))
    avi = os.path.join(work, 'base', 'videos', 'ci_mjpeg.avi')
    if not os.path.isfile(avi):
        return errors + ['motion JPEG video: ci_mjpeg.avi not written']
    width, height, total, chunks = read_avi(avi)
    sizes = [jpeg_size(chunk) for chunk in chunks]
    print('motion JPEG video: %dx%d, %d frames in the header, %d chunks, %d bytes in all'
          % (width, height, total, len(chunks), sum(len(chunk) for chunk in chunks)))
    if (width, height) != (640, 480):
        errors.append('motion JPEG video: %dx%d, expected 640x480' % (width, height))
    if len(chunks) < 5 or total != len(chunks):
        errors.append('motion JPEG video: %d chunks, %d frames in the header, expected the same 5 or more'
                      % (len(chunks), total))
    bad = [n for n, size in enumerate(sizes) if size != (640, 480)]
    if bad:
        errors.append('motion JPEG video: frames %s are not 640x480 JPEG images' % bad)
    return errors


# a console of 60 rows of 160 characters (scale 0.5 at 640x480), opened
# at once, where each character is a 48 byte stretch-pic command: more
# than the 256 KB of the frame's command list
FULL_CONSOLE = ['+set', 'con_scale', '0.5', '+set', 'con_height', '1', '+set', 'con_speed', '1000',
                '+exec', 'ci_full_console', '+toggleconsole', '+wait', '5']


def check_full_console_video(client, work):
    """Errors of a motion JPEG recording under a console that fills the
    render command list."""
    with open(os.path.join(work, 'base', 'ci_full_console.cfg'), 'w') as f:
        f.write(''.join('echo %s\n' % ('M' * 150) for _ in range(70)))
    args = VIDEO_SCENE + FULL_CONSOLE + ['+video', 'ci_full_console', '+wait', '5',
                                         '+screenshot', 'ci_full_console', '+wait', '5', '+toggleconsole',
                                         '+wait', '20', '+stopvideo', '+wait', '5', '+quit']
    rc, log = run_client(client, work, args)
    with open(os.path.join(work, 'client_full_console.log'), 'w') as f:
        f.write(log)
    errors = []
    if exit_error(rc):
        errors.append('full console video: ' + exit_error(rc))
    for marker in FATAL_MARKERS:
        if marker in log:
            errors.append('full console video: log contains %r' % marker)
    if "couldn't capture the frame" in log or 'not enough memory' in log:
        errors.append('full console video: the recording ended, or blamed memory')
    if "couldn't read the frame, screenshots/ci_full_console.jpg not written" not in log:
        errors.append('full console video: the screenshot that could not be read said nothing')
    if os.path.exists(os.path.join(work, 'base', 'screenshots', 'ci_full_console.jpg')):
        errors.append('full console video: ci_full_console.jpg written without its frame')
    avi = os.path.join(work, 'base', 'videos', 'ci_full_console.avi')
    if not os.path.isfile(avi):
        return errors + ['full console video: ci_full_console.avi not written']
    chunks = read_avi(avi)[3]
    dropped = [n for n, chunk in enumerate(chunks) if not chunk]
    images = [n for n, chunk in enumerate(chunks) if jpeg_size(chunk) == (640, 480)]
    print('full console video: %d chunks, frames %s dropped, %d JPEG images'
          % (len(chunks), dropped, len(images)))
    if not dropped:
        errors.append('full console video: no frame dropped (the console no longer fills the command list?)')
    if len(images) < 5 or images[-1] != len(chunks) - 1 or len(dropped) + len(images) != len(chunks):
        errors.append('full console video: the recording did not go on after the console closed')
    return errors


def check_video_capture_memory(client, work, fail_malloc):
    """Errors of the run where the renderer's video capture buffer fails."""
    frame = 642 * 480 * 3     # the renderer's; the client's is PAD(642 * 3, 4) * 480
    args = ['+set', 'r_mode', '-1', '+set', 'r_customwidth', '642', '+set', 'r_customheight', '480'] + \
        VIDEO_SCENE + ['+video', 'ci_nomem_mjpeg', '+wait', '10', '+stopvideo', '+wait', '5', '+quit']
    rc, log = run_client(client, work, args, {'LD_PRELOAD': fail_malloc, 'FAIL_MALLOC_MIN': str(frame),
                                               'FAIL_MALLOC_MAX': str(frame + 256)})
    with open(os.path.join(work, 'client_nomem_mjpeg.log'), 'w') as f:
        f.write(log)
    errors = []
    if exit_error(rc):
        errors.append('without video capture memory: ' + exit_error(rc))
    for marker in FATAL_MARKERS:
        if marker in log:
            errors.append('without video capture memory: log contains %r' % marker)
    if 'video: not enough memory' in log:
        errors.append('without video capture memory: the client buffer failed (test no longer meaningful)')
    if 'not enough memory to capture a 642x480 frame' not in log:
        errors.append('without video capture memory: no warning from the renderer')
    if log.count("video: couldn't capture the frame, ending the recording") != 1:
        errors.append('without video capture memory: the recording did not end once')
    avi = os.path.join(work, 'base', 'videos', 'ci_nomem_mjpeg.avi')
    if not os.path.isfile(avi):
        errors.append('without video capture memory: ci_nomem_mjpeg.avi not closed')
    elif read_avi(avi)[3]:
        errors.append('without video capture memory: ci_nomem_mjpeg.avi has frames')
    return errors


def check_gl_start_failure(client, work, gl_context_check):
    """Errors of the run where GL can't start: the fatal error shuts the
    renderer down before it ever had a context."""
    # no mode works with the dummy driver, not even the fallback (r_mode 3)
    rc, log = run_client(client, work, ['+set', 'r_mode', '4'],
                         {'SDL_VIDEODRIVER': 'dummy', 'LD_PRELOAD': gl_context_check})
    with open(os.path.join(work, 'client_nogl.log'), 'w') as f:
        f.write(log)
    errors = []
    if isinstance(rc, int) and rc < 0 or rc == 'timeout':
        errors.append('without GL: ' + exit_error(rc))
    if 'could not load OpenGL subsystem' not in log:
        errors.append('without GL: no fatal error (did a window start?)')
    if 'RE_Shutdown( 1 )' not in log:
        errors.append('without GL: the renderer was not shut down')
    for line in sorted(set(re.findall(r'GL call without a context: .*', log))):
        errors.append('without GL: ' + line)
    return errors


def main():
    if len(sys.argv) not in (4, 5, 6):
        sys.exit(__doc__)
    client, built, work = (os.path.abspath(a) for a in sys.argv[1:4])
    fail_malloc = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else None
    gl_context_check = os.path.abspath(sys.argv[5]) if len(sys.argv) > 5 else None
    subprocess.check_call([sys.executable, os.path.join(HERE, 'make_stub_base.py'), work, built])
    # native menu module next to the game directory
    for lib in glob.glob(os.path.join(built, 'jk2mvmenu_*')):
        shutil.copy(lib, work)

    rc, log = run_client(client, work, [
           '+wait', '60', '+toggleconsole', '+wait', '30', '+screenshot_tga', SHOTS[0], '+wait', '5',
           '+vid_restart', '+wait', '30', '+screenshot_tga', SHOTS[1], '+wait', '5',
           # both run in one frame, which ends with a single renderer
           # restart for them, before the wait and the quit
           '+preset', 'ultra', '+preset', 'classic', '+wait', '5',
           # the map load shuts the renderer down and closes the console
           '+map', 'ci_box', '+vid_restart', '+wait', '100', '+toggleconsole', '+wait', '30',
           '+screenshot_tga', SHOTS[2], '+wait', '5', '+quit'])
    with open(os.path.join(work, 'client.log'), 'w') as f:
        f.write(log)

    errors = []
    if exit_error(rc):
        errors.append(exit_error(rc))
    if log.count('----- finished R_Init -----') < 2:
        errors.append('the renderer did not restart (vid_restart)')
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
        # images kept from before a vid_restart come back white: the console
        # background, the fonts
        samples = range(0, len(pixels), step * 7)
        white = sum(1 for i in samples if min(pixels[i:i + 3]) >= 240) / float(len(samples))
        if white > 0.5:
            errors.append('%s is %d%% white: images lost by a vid_restart' % (name, white * 100))

    # from the defaults, ultra turns on the latched post-process cvars and
    # classic sets back exactly what ultra changed
    ultra, classic = preset_lines(log, 'ultra'), preset_lines(log, 'classic')
    if not ultra or not classic:
        errors.append('preset ultra or preset classic did not run')
    else:
        for preset, (_, others) in (('ultra', ultra), ('classic', classic)):
            if others:
                errors.append('preset %s: %s (unknown or protected cvar)' % (preset, '; '.join(others)))
        for cvar in ('r_fbo', 'r_hdr', 'r_bloom'):
            if ultra[0].get(cvar) != ('0', '1'):
                errors.append('preset ultra: %s %s, expected 0 -> 1' % (cvar, ultra[0].get(cvar)))
        undo = {cvar: (new, old) for cvar, (old, new) in ultra[0].items()}
        if classic[0] != undo:
            errors.append('preset classic did not undo ultra: %s' % sorted(set(classic[0].items()) ^ set(undo.items())))
        if log.count('restarting the renderer to apply the preset') != 2:
            errors.append('the presets did not ask for a renderer restart')
        print('preset ultra changed %d cvars, preset classic %d' % (len(ultra[0]), len(classic[0])))

    errors += check_motion_jpeg(client, work)
    errors += check_full_console_video(client, work)
    if fail_malloc:
        errors += check_capture_memory(client, work, fail_malloc)
        errors += check_video_capture_memory(client, work, fail_malloc)
    if gl_context_check:
        errors += check_gl_start_failure(client, work, gl_context_check)

    if errors:
        print('\n'.join(log.splitlines()[-60:]))
        print('client smoke test FAILED:\n  ' + '\n  '.join(errors))
        sys.exit(1)
    print('client smoke test passed: renderer up, console drawn, vid_restart, screenshots written, presets, '
          'motion JPEG video%s%s'
          % (', captures without memory' if fail_malloc else '',
             ', no GL call when GL cannot start' if gl_context_check else ''))


if __name__ == '__main__':
    main()
