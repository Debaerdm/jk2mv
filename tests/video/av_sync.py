#!/usr/bin/env python3
"""Audio/video sync check of the video capture, on the retail assets.

Records a demo in which the player fires the Bryar pistol four times, then
records it with `video` at 30 fps twice: with the game running at 125 fps,
faster than the video, and at 20 fps, slower than the video (as with a slow
encode). For each shot it finds the video frame with the muzzle flash and
the first sample of the shot's sound, and prints the sound's start minus the
start of that frame. It must be under one video frame in both recordings,
and the same in both: the real frame times must not matter.

The music is muted so that the shots start from silence. The Bryar's sound
begins with about 30 ms of near silence (the start of its MP3), so a sound
placed exactly on its frame measures about +30 ms; the audible shot comes
about 10 ms later.

Not part of ctest: it needs the retail assets and opens a window.

usage: av_sync.py [--keep] <jk2mvmp> <fs_basepath holding the retail base/> <work dir>
       av_sync.py --avi <file.avi>...
       av_sync.py --raw <frames> <width>x<height> <fps> <file.wav>

--keep keeps the AVIs (raw frames, about 200 MB each). --avi measures
existing AVIs. --raw measures video_mp4's temporary files: the bgr24 frames
as ffmpeg received them (with a stand-in ffmpeg that saves its input) and
the sound track written next to them.

Python 3 with numpy; MJPEG frames also need Pillow (the recordings made here
use raw frames).
"""

import os
import shutil
import struct
import subprocess
import sys

import numpy as np

# where the muzzle flash of the Bryar pistol is, as fractions of the frame
FLASH_REGION = (0.40, 0.80, 0.50, 0.95)     # left, right, top, bottom
FLASH_JUMP = 15.0       # mean luma rise of the region on the flash frame
SHOT_LEVEL = 0.4        # of the loudest sample: the shots, above their own echoes
SILENCE_RUN = 64        # samples at the background level that come before a shot
REFRACTORY = 0.5        # seconds between two shots, at least


def riff_chunks(data, start, end):
    i = start
    while i + 8 <= end:
        cid = data[i:i + 4]
        size = struct.unpack('<I', data[i + 4:i + 8])[0]
        yield cid, i + 8, size
        i += 8 + size + (size & 1)


def read_avi(path):
    """Frames, frame rate and sound of an AVI written by the `video` command."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] != b'RIFF' or data[8:12] != b'AVI ':
        raise ValueError('%s: not an AVI' % path)
    avi = {'frames': []}
    audio = []
    streams = []

    def walk(start, end):
        for cid, off, size in riff_chunks(data, start, end):
            if cid == b'LIST' and data[off:off + 4] == b'movi':
                for cid2, off2, size2 in riff_chunks(data, off + 4, off + size):
                    if cid2 == b'00dc':
                        avi['frames'].append(data[off2:off2 + size2])
                    elif cid2 == b'01wb':
                        audio.append(data[off2:off2 + size2])
            elif cid == b'LIST':
                walk(off + 4, off + size)
            elif cid == b'avih':
                avi['width'], avi['height'] = struct.unpack('<II', data[off + 32:off + 40])
            elif cid == b'strh':
                scale, rate = struct.unpack('<II', data[off + 20:off + 28])
                streams.append({'type': data[off:off + 4], 'scale': scale, 'rate': rate})
            elif cid == b'strf' and streams[-1]['type'] == b'vids':
                streams[-1]['compression'] = data[off + 16:off + 20]
            elif cid == b'strf' and streams[-1]['type'] == b'auds':
                _, channels, rate, _, _, bits = struct.unpack('<HHIIHH', data[off:off + 16])
                streams[-1].update(channels=channels, samplerate=rate, bits=bits)

    walk(12, len(data))
    video = [s for s in streams if s['type'] == b'vids'][0]
    sound = [s for s in streams if s['type'] == b'auds']
    if not sound or sound[0]['bits'] != 16:
        raise ValueError('%s: no 16-bit sound track' % path)
    avi['fps'] = video['rate'] / float(video['scale'])
    avi['mjpeg'] = video['compression'] == b'MJPG'
    avi['rate'] = sound[0]['samplerate']
    avi['pcm'] = np.frombuffer(b''.join(audio), np.int16).reshape(-1, sound[0]['channels'])
    return avi


def read_wav(path):
    """Sample rate and samples (n x channels) of a 16-bit PCM WAV file."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('%s: not a WAV file' % path)
    fmt = None
    for cid, off, size in riff_chunks(data, 12, len(data)):
        if cid == b'fmt ':
            fmt = struct.unpack('<HHIIHH', data[off:off + 16])
        elif cid == b'data' and fmt:
            if fmt[0] != 1 or fmt[5] != 16:
                raise ValueError('%s: not 16-bit PCM' % path)
            # 0xFFFFFFFF (past 4 GB) means: up to the end of the file
            size = min(size, len(data) - off)
            size -= size % (2 * fmt[1])
            return fmt[2], np.frombuffer(data[off:off + size], np.int16).reshape(-1, fmt[1])
    raise ValueError('%s: no fmt or data chunk' % path)


def frame_luma(frame, width, height, mjpeg, padding):
    """Luma of a frame, top row first."""
    if mjpeg:
        import io
        from PIL import Image
        rgb = np.asarray(Image.open(io.BytesIO(frame)).convert('RGB'), np.float32)
        return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114
    # bottom-up BGR rows, padded to a multiple of padding bytes
    stride = (width * 3 + padding - 1) // padding * padding
    bgr = np.frombuffer(frame, np.uint8, stride * height).reshape(height, stride)
    bgr = bgr[::-1, :width * 3].reshape(height, width, 3).astype(np.float32)
    return bgr[..., 2] * 0.299 + bgr[..., 1] * 0.587 + bgr[..., 0] * 0.114


def region_levels(frames, width, height, mjpeg=False, padding=4):
    """Mean luma of the muzzle flash region, per frame."""
    left, right, top, bottom = FLASH_REGION
    levels = []
    for frame in frames:
        y = frame_luma(frame, width, height, mjpeg, padding)
        levels.append(y[int(height * top):int(height * bottom), int(width * left):int(width * right)].mean())
    return np.array(levels)


def flash_frames(levels, fps):
    """Frames where the muzzle flash appears: the region gets much brighter."""
    found, skip = [], int(REFRACTORY * fps)
    k = 1
    while k < len(levels):
        if levels[k] - levels[k - 1] > FLASH_JUMP:
            found.append(k)
            k += skip
        else:
            k += 1
    return found


def sound_starts(pcm, rate):
    """First sample of each shot: the loud part, then back to where the
    sound rises from the background before it."""
    level = np.abs(pcm.astype(np.int32)).max(axis=1)
    loud = np.nonzero(level > SHOT_LEVEL * level.max())[0]
    starts = []
    for s in loud:
        if starts and s - starts[-1][1] < REFRACTORY * rate:
            continue
        # the background: the quiet between the previous shot's echoes and this one
        before = level[max(0, s - int(0.25 * rate)):s]
        floor = np.percentile(before, 10) if len(before) else 0
        # the last run of SILENCE_RUN samples at the background level, within
        # 0.1 s (over music, there may be none: the loud part then)
        quiet = 0
        start = s
        while start > max(0, s - int(0.1 * rate)) and quiet < SILENCE_RUN:
            start -= 1
            quiet = quiet + 1 if level[start] <= floor else 0
        starts.append((start + SILENCE_RUN if quiet == SILENCE_RUN else int(s), int(s)))
    return starts


def measure(levels, fps, pcm, rate):
    """For each shot: its flash frame, and its sound's start and loud part
    minus the start of that frame, in ms."""
    shots = []
    starts = sound_starts(pcm, rate)
    for k in flash_frames(levels, fps):
        for start, loud in starts:
            if abs(start / float(rate) - k / fps) < REFRACTORY / 2:
                shots.append((k, (start / float(rate) - k / fps) * 1000.0, (loud / float(rate) - k / fps) * 1000.0))
                break
    return shots


def report(name, shots):
    print('%s: %d shots, sound start minus flash frame (ms): %s   (loud part: %s)' % (
        name, len(shots), ' '.join('%+.1f' % s[1] for s in shots), ' '.join('%+.1f' % s[2] for s in shots)))
    return [s[1] for s in shots]


def run_game(client, basepath, home, commands, timeout, files=()):
    """Runs the client on a fresh fs_homepath, the commands in a cfg (which
    should end with quit: the run is killed after timeout seconds)."""
    if os.path.exists(home):
        shutil.rmtree(home)
    os.makedirs(os.path.join(home, 'base', 'demos'))
    for src, dst in files:
        shutil.copy(src, os.path.join(home, 'base', dst))
    with open(os.path.join(home, 'base', 'avsync.cfg'), 'w') as f:
        f.write('\n'.join(commands) + '\n')
    sets = []
    for c in commands:
        words = c.split(None, 2)
        if len(words) == 3 and words[0] == 'set':
            sets += ['+set', words[1], words[2]]
    # windowed, without the mouse, on the loopback only
    cmd = [client, '+set', 'fs_basepath', basepath, '+set', 'fs_homepath', home, '+set', 'logfile', '2',
           '+set', 'com_introplayed', '1', '+set', 'net_ip', '127.0.0.1', '+set', 'r_fullscreen', '0',
           '+set', 'r_mode', '3', '+set', 'in_mouse', '0', '+set', 'in_nograb', '1'] + sets + ['+exec', 'avsync.cfg']
    try:
        subprocess.run(cmd, cwd=os.path.dirname(client), timeout=timeout)
    except subprocess.TimeoutExpired:
        print('%s: timed out' % home)


SHOT = ['+attack', 'wait 4', '-attack', 'wait 375']


def record_demo(client, basepath, work):
    home = os.path.join(work, 'record')
    # g_gametype 1: no force setup menu; 'wait' counts half frames
    run_game(client, basepath, home,
             ['set g_gametype 1', 'set com_maxfps 125', 'devmap ffa_bespin', 'wait 1500', 'team free',
              'wait 250', 'weapon 2', 'wait 250', 'record avsync', 'wait 500'] + SHOT * 4 + ['stoprecord', 'quit'],
             120)
    demo = os.path.join(home, 'base', 'demos', 'avsync.dm_16')
    if not os.path.exists(demo):
        sys.exit('the demo was not recorded, see %s' % os.path.join(home, 'base', 'qconsole.log'))
    return demo


def record_video(client, basepath, work, demo, name, maxfps):
    home = os.path.join(work, name)
    # the demo command loads the map: the video starts as the demo plays,
    # before the first shot, and nextdemo quits when it ends (nothing may
    # wait in the command buffer then: quit would come after)
    run_game(client, basepath, home,
             ['set com_maxfps 125', 'set cl_aviFrameRate 30', 'set cl_aviMotionJpeg 0', 'set s_musicvolume 0',
              'set nextdemo quit', 'demo avsync', 'wait 40', 'video ' + name, 'set com_maxfps %d' % maxfps],
             120, files=[(demo, os.path.join('demos', 'avsync.dm_16'))])
    path = os.path.join(home, 'base', 'videos', name + '.avi')
    if not os.path.exists(path):
        sys.exit('no video, see %s' % os.path.join(home, 'base', 'qconsole.log'))
    return path


def check_avi(path):
    avi = read_avi(path)
    levels = region_levels(avi['frames'], avi['width'], avi['height'], avi['mjpeg'])
    return avi['fps'], report(os.path.basename(path), measure(levels, avi['fps'], avi['pcm'], avi['rate']))


def main():
    args = sys.argv[1:]
    if args[:1] == ['--avi'] and len(args) > 1:
        for path in args[1:]:
            check_avi(path)
        return
    if args[:1] == ['--raw'] and len(args) == 5:
        width, height = (int(v) for v in args[2].split('x'))
        fps = float(args[3])
        with open(args[1], 'rb') as f:
            raw = f.read()
        size = width * height * 3
        frames = [raw[i:i + size] for i in range(0, len(raw) - size + 1, size)]
        rate, pcm = read_wav(args[4])
        report(os.path.basename(args[1]), measure(region_levels(frames, width, height, padding=1), fps, pcm, rate))
        return
    keep = args[:1] == ['--keep']
    if keep:
        args = args[1:]
    if len(args) != 3:
        sys.exit(__doc__)
    client, basepath, work = (os.path.abspath(a) for a in args)
    demo = record_demo(client, basepath, work)
    results = []
    for name, maxfps in (('avsync125', 125), ('avsync20', 20)):
        path = record_video(client, basepath, work, demo, name, maxfps)
        results.append((name,) + check_avi(path))
        if not keep:
            os.remove(path)
    errors = []
    for name, fps, offsets in results:
        if len(offsets) < 4:
            errors.append('%s: %d shots found instead of 4' % (name, len(offsets)))
        off = [o for o in offsets if abs(o) >= 1000.0 / fps]
        if off:
            errors.append('%s: sound more than one frame off its picture: %s ms' % (
                name, ', '.join('%+.1f' % o for o in off)))
    a, b = results[0][2], results[1][2]
    if len(a) == len(b) and any(abs(x - y) > 1.0 for x, y in zip(a, b)):
        errors.append('the offsets depend on the real frame rate')
    for e in errors:
        print('FAIL: ' + e)
    if errors:
        sys.exit(1)
    print('OK')


if __name__ == '__main__':
    main()
