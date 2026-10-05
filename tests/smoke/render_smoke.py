#!/usr/bin/env python3
"""Headless renderer smoke test of the post-processing (r_fbo, r_hdr, r_bloom).

Draws the generated ci_box room with the testscene command (no cgame or
retail assets needed) under several settings, with SDL's offscreen video
driver and software OpenGL, and checks the screenshots:
  - the scene shows in place of the full screen main menu
  - rendering offscreen (r_fbo 1) gives the same image as the back buffer,
    and the same dynamic glow within its extra precision
  - bloom brightens the wall around the lamp and leaves the rest alone
  - in HDR the bloom of the red strips stays red instead of washing out
  - every effect at once with MSAA runs without a GL error
  - per-pixel dynamic lights (r_dlightMode 1) light the wall and floor near
    the light without the classic vertical smear above it
  - with r_fbo the color grade tints the 3D view as before but no longer the
    console drawn over it
  - past 32 dynamic lights, r_dlightPriority keeps the visible one that the
    classic first come first served limit drops
  - video records the spinning scene at its fixed frame rate and stops with
    testscene off, which brings the menu back
  - r_renderScale 2, 1.5 and 0.5 render at that scale times the window's
    size (screenshots included), also when the scale changes through a
    renderer restart that keeps the window, and look the same once brought
    back to it, bloom halo included; the window shows the frame filtered as
    designed (r_screenshotWindowSize reads it)

usage: render_smoke.py <jk2mvmp> <directory holding the built base/> <work dir>
"""

import glob
import math
import os
import re
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True   # no __pycache__ in the source tree
from client_smoke import FATAL_MARKERS, exit_error, read_tga  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SCENE = ['+testscene', 'ci_box', '-256', '0', '96']
# close to the wall, with a dynamic light between the camera and the wall
DLIGHT_SCENE = ['+testscene', 'ci_box', '200', '0', '96', '0', '0', 'dlight']

RUNS = {
    'classic': [],
    'fbo': ['+set', 'r_fbo', '1'],
    'glow': ['+set', 'r_DynamicGlow', '1'],
    'glow_fbo': ['+set', 'r_DynamicGlow', '1', '+set', 'r_fbo', '1'],
    'bloom': ['+set', 'r_fbo', '1', '+set', 'r_bloom', '1'],
    'hdr_bloom': ['+set', 'r_fbo', '1', '+set', 'r_hdr', '1', '+set', 'r_bloom', '1'],
    'all': ['+set', 'r_fbo', '1', '+set', 'r_hdr', '1', '+set', 'r_bloom', '1', '+set', 'r_DynamicGlow', '1',
            '+set', 'r_ext_multisample', '4'],
    'scale2': ['+set', 'r_fbo', '1', '+set', 'r_bloom', '1', '+set', 'r_renderScale', '2'],
    'scale05': ['+set', 'r_fbo', '1', '+set', 'r_bloom', '1', '+set', 'r_renderScale', '0.5'],
    # started at 2, then 1.5 from the renderer restart of testscene, which
    # keeps the window. '+set' lines also apply before the renderer starts,
    # the last one winning; seta only runs in its turn.
    'scale15': ['+set', 'r_fbo', '1', '+set', 'r_renderScale', '2', '+seta', 'r_renderScale', '1.5'],
}
# r_renderScale run: (render scale, run at scale 1 it must look like)
SCALE_RUNS = {'scale2': (2, 'bloom'), 'scale05': (0.5, 'bloom'), 'scale15': (1.5, 'fbo')}
# with the console open over the bottom of the view
CONSOLE = ['+toggleconsole', '+wait', '40']
GRADE_RUNS = {
    'console': [],
    'noir_classic': ['+set', 'r_colorGrade', 'noir'],
    'noir_view': ['+set', 'r_colorGrade', 'noir', '+set', 'r_fbo', '1'],
}
DLIGHT_RUNS = {
    'nodlight': (['+testscene', 'ci_box', '200', '0', '96', '0', '0'], []),
    'dlight_classic': (DLIGHT_SCENE, ['+set', 'r_dlightMode', '0']),
    'dlight_pixel': (DLIGHT_SCENE, ['+set', 'r_dlightMode', '1']),
    # the light in front comes after 40 lights behind the camera
    'crowd_classic': (DLIGHT_SCENE[:-1] + ['dlights'], ['+set', 'r_dlightPriority', '0']),
    'crowd': (DLIGHT_SCENE[:-1] + ['dlights'], ['+set', 'r_dlightPriority', '1']),
}
# uncompressed video of the spinning scene from its first frame: the camera
# starts 20 degrees right of the lamp and turns left, so the lamp crosses
# the view; the run ends on the menu after testscene off
VIDEO_ARGS = ['+set', 'cl_aviMotionJpeg', '0', '+set', 'cl_aviFrameRate', '30']
VIDEO_SCENE = ['+testscene', 'ci_box', '-256', '0', '96', '-20', '0', 'spin', '+video', 'ci_spin']
VIDEO_AFTER = ['+testscene', 'off', '+wait', '40']

# 640x480 screen areas, top-down
WALL_NEAR_LAMP = (350, 175, 375, 200)
WALL_NEAR_RED = (205, 170, 222, 220)
# between the red strips and the lamp, where a bloom of the wrong size shows most
WALL_BETWEEN_LIGHTS = (256, 175, 290, 200)
FLOOR = (100, 400, 540, 470)
# in the dynamic light scene: the wall right behind the light, the wall far
# above it (out of its radius), the floor under it
WALL_AT_LIGHT = (312, 274, 328, 288)
WALL_ABOVE_LIGHT = (312, 6, 328, 22)
FLOOR_UNDER_LIGHT = (300, 440, 340, 470)
# with the console open: its background, and the view under it
CONSOLE_BACK = (100, 60, 540, 200)
VIEW_UNDER_CONSOLE = (100, 400, 540, 470)
# pure green in the stub's full screen main menu (make_stub_base.py), under
# the half console and above the menu's bottom band
MENU_PROBE = (280, 300, 360, 380)


class Shot:
    def __init__(self, path):
        self.width, self.height, bpp, self.pixels = read_tga(path)
        self.step = bpp // 8

    @classmethod
    def frame(cls, width, height, pixels):
        """An uncompressed video frame, also bottom-up BGR rows."""
        shot = cls.__new__(cls)
        shot.width, shot.height, shot.step, shot.pixels = width, height, 3, pixels
        return shot

    def rgb(self, x, y):
        # bottom-up rows, BGR(A)
        i = ((self.height - 1 - y) * self.width + x) * self.step
        return self.pixels[i + 2], self.pixels[i + 1], self.pixels[i]

    def mean(self, box):
        x0, y0, x1, y1 = box
        total = [0, 0, 0]
        for y in range(y0, y1):
            for x in range(x0, x1):
                for c, v in enumerate(self.rgb(x, y)):
                    total[c] += v
        n = (x1 - x0) * (y1 - y0)
        return [t / n for t in total]

    def shows_menu(self):
        r, g, b = self.mean(MENU_PROBE)
        return g > 200 and r < 20 and b < 20

    def lamp_yaw(self):
        """Camera yaw in the video scene, from where the lamp shows: it is
        the only pure white, straight ahead at yaw 0, and the view is 90
        degrees wide. None without the lamp."""
        xs = [x for y in range(100, 260, 2) for x in range(0, self.width, 2) if min(self.rgb(x, y)) >= 250]
        if not xs:
            return None
        half = self.width / 2
        return math.degrees(math.atan((sum(xs) / len(xs) - half) / half))


def read_avi(path):
    """The video frames of an uncompressed engine AVI."""
    with open(path, 'rb') as f:
        data = f.read()
    avih = data.find(b'avih')
    width, height = struct.unpack('<2I', data[avih + 40:avih + 48])
    frames = []
    pos = data.find(b'movi') + 4
    while pos + 8 <= len(data):
        chunk, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        if chunk == b'idx1':
            break
        if chunk == b'00dc':
            frames.append(Shot.frame(width, height, data[pos + 8:pos + 8 + size]))
        pos += 8 + size + (size & 1)
    return frames


def compare(a, b):
    """Number of differing pixels and the largest channel difference."""
    differing = largest = 0
    for i in range(0, min(len(a.pixels), len(b.pixels)), a.step):
        d = max(abs(a.pixels[i + c] - b.pixels[i + c]) for c in range(3))
        if d:
            differing += 1
            largest = max(largest, d)
    return differing, largest


def shown_shot(name):
    """Commands writing render_<name>_shown: the window, as the frame of
    r_renderScale is shown in it (seta, see 'scale15')."""
    return ['+seta', 'r_screenshotWindowSize', '1', '+screenshot_tga', 'render_%s_shown' % name, '+wait', '5',
            '+seta', 'r_screenshotWindowSize', '0']


def offscreen_inits(log):
    """(render size, window size it is shown at or None) of each r_fbo start."""
    inits = []
    for inside in re.findall(r'rendering offscreen \(([^)]*)\)', log):
        shown = re.search(r'scaled to (\d+x\d+)', inside)
        inits.append((inside.split(',')[0].strip(), shown.group(1) if shown else None))
    return inits


def show_taps(shown, frame):
    """For each window pixel along one axis, the texels of the frame that the
    show pass of r_renderScale averages, and their weights: two bilinear
    taps a quarter of the pixel's footprint either side of its center when
    shrinking (R_RenderScaleBoxOffset), both on the center when enlarging,
    kept between the centers of the edge texels."""
    ratio = frame / shown
    offset = ratio / 4 if ratio > 1 else 0.0
    taps = []
    for i in range(shown):
        weights = {}
        for s in ((i + 0.5) * ratio - offset, (i + 0.5) * ratio + offset):
            s = min(max(s, 0.5), frame - 0.5) - 0.5
            i0 = int(s)
            for texel, w in ((i0, 1 - (s - i0)), (min(i0 + 1, frame - 1), s - i0)):
                weights[texel] = weights.get(texel, 0.0) + w / 2
        taps.append(list(weights.items()))
    return taps


def compare_shown(frame, window, step=3):
    """Largest and mean channel difference between the window and the frame
    filtered as the show pass does it, on every step-th pixel each way (rows
    are bottom-up in both, as in GL)."""
    xs, ys = show_taps(window.width, frame.width), show_taps(window.height, frame.height)
    largest = total = count = 0
    for y in range(0, window.height, step):
        for x in range(1, window.width, step):
            expected = [0.0, 0.0, 0.0]
            for ty, wy in ys[y]:
                for tx, wx in xs[x]:
                    i = (ty * frame.width + tx) * frame.step
                    for c in range(3):
                        expected[c] += wx * wy * frame.pixels[i + c]
            j = (y * window.width + x) * window.step
            for c in range(3):
                d = abs(window.pixels[j + c] - expected[c])
                largest = max(largest, d)
                total += d
                count += 1
    return largest, total / count


def run(client, work, name, args, scene=SCENE, after=()):
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1')
    for cfg in ('jk2mvconfig.cfg', 'jk2mvglobal.cfg'):   # archived r_ settings of the previous run
        path = os.path.join(work, 'base', cfg)
        if os.path.exists(path):
            os.remove(path)
    cmd = [client, '+set', 'fs_basepath', work, '+set', 'fs_homepath', work,
           '+set', 'r_allowsoftwaregl', '1', '+set', 'r_fullscreen', '0', '+set', 'r_mode', '3',
           '+set', 's_initsound', '0', '+set', 'com_introplayed', '1', '+set', 'con_notifytime', '0',
           '+set', 'r_ignoreGLErrors', '0'] + args + \
          ['+wait', '60'] + scene + ['+wait', '30'] + list(after) + [ '+screenshot_tga', 'render_' + name, '+wait', '5', '+quit']
    try:
        proc = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=180)
        rc, out = proc.returncode, proc.stdout.decode('latin-1')
    except subprocess.TimeoutExpired as e:
        rc, out = 'timeout', (e.stdout or b'').decode('latin-1')
    log = re.sub(r'\x1b\[[0-9;]*m', '', out)
    log = re.sub(r'^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d ', '', log, flags=re.M)
    with open(os.path.join(work, 'render_%s.log' % name), 'w') as f:
        f.write(log)

    errors = []
    if exit_error(rc):
        errors.append('%s: %s' % (name, exit_error(rc)))
    for marker in FATAL_MARKERS:
        if marker in log:
            errors.append('%s: log contains %r' % (name, marker))
    if '+set r_fbo 1' in ' '.join(args) and 'rendering offscreen' not in log:
        errors.append('%s: r_fbo 1 did not render offscreen' % name)
    path = os.path.join(work, 'base', 'screenshots', 'render_%s.tga' % name)
    if not os.path.isfile(path):
        errors.append('%s: missing screenshot' % name)
        return None, errors, log
    return Shot(path), errors, log


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    client, built, work = (os.path.abspath(a) for a in sys.argv[1:])
    subprocess.check_call([sys.executable, os.path.join(HERE, 'make_stub_base.py'), work, built])
    for lib in glob.glob(os.path.join(built, 'jk2mvmenu_*')):
        shutil.copy(lib, work)

    shots, shown, logs, errors, last_log = {}, {}, {}, [], ''
    runs = [(name, args, SCENE, shown_shot(name) if name in SCALE_RUNS else ()) for name, args in RUNS.items()]
    runs += [(name, args, scene, ()) for name, (scene, args) in DLIGHT_RUNS.items()]
    runs += [(name, args, SCENE, CONSOLE) for name, args in GRADE_RUNS.items()]
    runs.append(('video', VIDEO_ARGS, VIDEO_SCENE, VIDEO_AFTER))
    for name, args, scene, after in runs:
        shot, errs, log = run(client, work, name, args, scene, after)
        if name in SCALE_RUNS:
            path = os.path.join(work, 'base', 'screenshots', 'render_%s_shown.tga' % name)
            if os.path.isfile(path):
                shown[name] = Shot(path)
            else:
                errs.append('%s: missing screenshot of the window' % name)
        errors += errs
        logs[name] = log
        if errs:
            last_log = log
        if shot:
            shots[name] = shot

    def check(condition, message):
        if not condition:
            errors.append(message)

    # a frame that reopens the full screen main menu hides the scene, as the
    # retail menus did (the video run ends on the menu)
    covered = sorted(name for name, shot in shots.items() if name != 'video' and shot.shows_menu())
    check(not covered, 'the main menu hides the test scene: %s' % ', '.join(covered))

    if len(shots) == len(runs):
        # the offscreen glow keeps 16 bits between its blur passes where the
        # copies of the classic one go through the 8-bit back buffer
        for a, b, max_pixels, max_diff in (('classic', 'fbo', 640 * 480 // 200, 2),
                                           ('glow', 'glow_fbo', 640 * 480 // 50, 4)):
            differing, largest = compare(shots[a], shots[b])
            print('%s vs %s: %d pixels differ, by up to %d' % (a, b, differing, largest))
            check(differing <= max_pixels and largest <= max_diff,
                  '%s and %s differ: %d pixels, up to %d' % (a, b, differing, largest))

        classic = shots['classic']
        wall = classic.mean(WALL_NEAR_LAMP)
        check(wall[0] > 60, 'the test scene was not drawn (wall %s)' % wall)
        for name in ('bloom', 'hdr_bloom', 'all'):
            floor_diff = max(abs(p - q) for p, q in zip(shots[name].mean(FLOOR), classic.mean(FLOOR)))
            check(floor_diff <= 1.0, '%s changed the floor by %.1f' % (name, floor_diff))
        bloom = shots['bloom'].mean(WALL_NEAR_LAMP)
        print('wall near the lamp: classic %.1f, bloom %.1f' % (wall[0], bloom[0]))
        check(bloom[0] >= wall[0] + 4, 'no bloom around the lamp (%.1f -> %.1f)' % (wall[0], bloom[0]))

        ldr = shots['bloom'].mean(WALL_NEAR_RED)
        hdr = shots['hdr_bloom'].mean(WALL_NEAR_RED)
        print('red halo (red - green): bloom %.1f, HDR bloom %.1f' % (ldr[0] - ldr[1], hdr[0] - hdr[1]))
        check(hdr[0] - hdr[1] >= ldr[0] - ldr[1] + 8, 'HDR bloom is not redder than 8-bit bloom')

        glow, every = shots['glow'].mean(WALL_NEAR_LAMP), shots['all'].mean(WALL_NEAR_LAMP)
        check(every[0] > glow[0], 'glow + bloom is not brighter than glow alone')

        # the light is blue: compare the blue channel with the unlit scene
        unlit = shots['nodlight']
        def gain(name, box):
            return shots[name].mean(box)[2] - unlit.mean(box)[2]
        print('dynamic light, blue gain at / above the light / floor: classic %.1f %.1f %.1f, per pixel %.1f %.1f %.1f'
              % (gain('dlight_classic', WALL_AT_LIGHT), gain('dlight_classic', WALL_ABOVE_LIGHT),
                 gain('dlight_classic', FLOOR_UNDER_LIGHT), gain('dlight_pixel', WALL_AT_LIGHT),
                 gain('dlight_pixel', WALL_ABOVE_LIGHT), gain('dlight_pixel', FLOOR_UNDER_LIGHT)))
        for name in ('dlight_classic', 'dlight_pixel'):
            check(gain(name, WALL_AT_LIGHT) >= 30, '%s does not light the wall' % name)
            check(gain(name, FLOOR_UNDER_LIGHT) >= 30, '%s does not light the floor' % name)
        check(gain('dlight_classic', WALL_ABOVE_LIGHT) >= 10, 'the classic light no longer smears (test scene changed?)')
        check(gain('dlight_pixel', WALL_ABOVE_LIGHT) <= 2, 'the per-pixel light reaches past its radius')
        print('41st dynamic light, blue gain on the wall: first come first served %.1f, by priority %.1f'
              % (gain('crowd_classic', WALL_AT_LIGHT), gain('crowd', WALL_AT_LIGHT)))
        check(gain('crowd_classic', WALL_AT_LIGHT) <= 1, 'the classic limit kept the 41st light (test no longer meaningful)')
        check(abs(gain('crowd', WALL_AT_LIGHT) - gain('dlight_classic', WALL_AT_LIGHT)) <= 1,
              'r_dlightPriority did not keep the visible light')

        plain, graded, view = shots['console'], shots['noir_classic'], shots['noir_view']
        print('console background: plain %s, noir %s, noir with r_fbo %s' % (
            [round(c) for c in plain.mean(CONSOLE_BACK)], [round(c) for c in graded.mean(CONSOLE_BACK)],
            [round(c) for c in view.mean(CONSOLE_BACK)]))
        check(max(abs(p - q) for p, q in zip(view.mean(CONSOLE_BACK), plain.mean(CONSOLE_BACK))) <= 1,
              'with r_fbo the grade still tints the console')
        check(max(abs(p - q) for p, q in zip(graded.mean(CONSOLE_BACK), plain.mean(CONSOLE_BACK))) >= 5,
              'noir does not change the console without r_fbo (test no longer meaningful)')
        view_floor, graded_floor = view.mean(VIEW_UNDER_CONSOLE), graded.mean(VIEW_UNDER_CONSOLE)
        check(max(abs(p - q) for p, q in zip(view_floor, graded_floor)) <= 3,
              'the graded view differs with r_fbo: %s vs %s' % (view_floor, graded_floor))
        check(max(view_floor) - min(view_floor) <= 2, 'noir left color in the view: %s' % view_floor)

        # the recording holds the spinning scene, and stops with it
        avi = os.path.join(work, 'base', 'videos', 'ci_spin.avi')
        frames = read_avi(avi) if os.path.isfile(avi) else []
        menu = [n for n, frame in enumerate(frames) if frame.shows_menu()]
        yaws = [frame.lamp_yaw() for frame in frames]
        print('video: %d frames, camera yaw %s' % (len(frames), ' '.join(
            '%.1f' % yaw if yaw is not None else '-' for yaw in yaws)))
        check(len(frames) >= 5, 'video: %d frames recorded' % len(frames))
        check(not menu, 'video: frames %s show the main menu, not the scene' % menu)
        if len(frames) >= 5 and None not in yaws:
            # 90 degrees per second of client time, which the video steps by
            # 1/30 s per frame whatever the real frame rate
            step = (yaws[-1] - yaws[0]) / (len(frames) - 1)
            print('video: spin turns the camera %.2f degrees per frame' % step)
            check(abs(step - 3) <= 0.3, 'video: spin turns %.2f degrees per frame instead of 3' % step)
        elif frames:
            check(False, 'video: no lamp in frames %s' % [n for n, yaw in enumerate(yaws) if yaw is None])
        check(shots['video'].shows_menu(), 'testscene off did not bring the main menu back')

        # r_renderScale: drawn at the scale times the window's size, shown at
        # the window's size, where it looks like scale 1. Each renderer start
        # scales the window's size, the restart of testscene too, which keeps
        # the window (rather than the render size it had). Around the lamp,
        # the bloom halo is as bright as at scale 1 (a chain of the render
        # size would make it half as wide at scale 2, 3.6 darker next to the
        # lamp, and twice as wide at 0.5, 3.0 brighter between the lights).
        # The window shows the frame filtered by the show pass.
        for name, (scale, ref) in SCALE_RUNS.items():
            shot = shots[name]
            size = (int(640 * scale), int(480 * scale))
            inits = offscreen_inits(logs[name])
            print('%s: renderer starts %s' % (name, inits))
            check(len(inits) >= 2 and all(window == '640x480' for _, window in inits),
                  '%s: each renderer start should scale the 640x480 window: %s' % (name, inits))
            check(inits and inits[-1][0] == '%dx%d' % size,
                  '%s: the last renderer start is not at %dx%d: %s' % (name, size[0], size[1], inits))
            check((shot.width, shot.height) == size,
                  '%s: screenshot of %dx%d instead of %dx%d' % (name, shot.width, shot.height, size[0], size[1]))
            if (shot.width, shot.height) != size:
                continue
            for area, box in (('wall near the lamp', WALL_NEAR_LAMP), ('wall between the lights', WALL_BETWEEN_LIGHTS),
                              ('floor', FLOOR)):
                scaled = tuple(int(v * scale) for v in box)
                diff = max(abs(p - q) for p, q in zip(shot.mean(scaled), shots[ref].mean(box)))
                print('%s: %s within %.1f of %s' % (name, area, diff, ref))
                check(diff <= 2.0, '%s: the %s differs from %s by %.1f' % (name, area, ref, diff))
            window = shown.get(name)
            if not window or (window.width, window.height) != (640, 480):
                check(False, '%s: the screenshot of the window is not 640x480' % name)
                continue
            largest, mean = compare_shown(shot, window)
            print('%s: the window shows the filtered frame within %.2f (mean %.2f)' % (name, largest, mean))
            check(largest <= 2.5 and mean <= 0.75,
                  '%s: the window differs from the filtered frame: up to %.2f, mean %.2f' % (name, largest, mean))

    if errors:
        print('\n'.join(last_log.splitlines()[-60:]))
        print('render smoke test FAILED:\n  ' + '\n  '.join(errors))
        sys.exit(1)
    print('render smoke test passed: offscreen path identical, bloom, HDR, per-pixel lights, view grading and '
          'test scene video and render scale work, no GL error')


if __name__ == '__main__':
    main()
