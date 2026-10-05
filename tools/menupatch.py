#!/usr/bin/env python3
"""menupatch.py -- show and edit jk2mv's .menu_patch files.

jk2mv changes the retail menus when it loads them (MV_MenuPatchFile,
src/botlib/l_script.cpp): assetsmv.pk3 holds <menu>_patch next to the menu
path, made of commands on the lines of the ORIGINAL menu:

  ORI_HASH <crc32>          the patch only applies to the menu with this CRC32;
                            another one loads unpatched, and the console says
                            "patching skipped: hash mismatch"
  LINES_DELETE <a> [<b>]    removes original lines a to b
  LINES_INSERT <n> [        the lines up to "]" go after original line n
  ]
  LINES_REPLACE <n> [       the lines up to "]" replace original lines n, n+1...
  ]

Line numbers always refer to the original menu, so hunks don't shift each
other, but a hunk can't change a line that another one inserted, and an
INSERT applied later lands before the lines earlier ones inserted after the
same line. A patch has nothing else, not even comments: the engine would
reject all of it ("patching failed: syntax error in patchfile").

usage:
  menupatch.py show <patch> [-o <out.menu>] [--base <dir> | --menu <file>]
      Writes the patched menu (to stdout by default), after checking that
      every hunk applies like in the engine. The original menu is the one
      whose CRC32 is the ORI_HASH, from --menu or from the pk3s of --base (the
      base folder of a JK2 install, else $JK2_BASE).
  menupatch.py update <patch> <edited.menu> [--base <dir> | --menu <file>]
      Rewrites the patch so it gives edited.menu, with the smallest change to
      the patch file: lines that come from the patch are edited in place, and
      only the original lines that change get new hunks.

To change a patched menu: show it into a file, edit that file like any
menu, update the patch from it, rebuild assetsmv.pk3 and check that the
game's console says "patching menu file ..." without "failed" or "skipped".
"""

import argparse
import difflib
import os
import re
import sys
import zipfile
import zlib


class PatchError(Exception):
    pass


def lineize(text):
    """Lines like MV_MenuLineizeGetline: \\n, \\r\\n and \\r end a line, and a
    last line without one counts."""
    lines = re.split(r'\r\n|\r|\n', text)
    if lines[-1] == '':
        lines.pop()
    return lines


def parse_hunks(plines):
    """[(command, args, first, last)]: first and last are the indexes of the
    command line and, for INSERT and REPLACE, of the closing ']'."""
    hunks, i = [], 0
    while i < len(plines):
        cmd = plines[i].split()
        if cmd:
            if cmd[0] in ('ORI_HASH', 'LINES_DELETE'):
                hunks.append((cmd[0], cmd[1:], i, i))
            elif cmd[0] in ('LINES_INSERT', 'LINES_REPLACE'):
                j = i + 1
                while j < len(plines) and plines[j] != ']':
                    j += 1
                if j == len(plines):
                    raise PatchError('line %d: no "]" after %s' % (i + 1, plines[i]))
                hunks.append((cmd[0], cmd[1:], i, j))
                i = j
            else:
                raise PatchError('line %d: unknown command %r' % (i + 1, plines[i]))
        i += 1
    return hunks


def apply(orig, plines):
    """The patched menu as [(text, origin)]: origin is ('orig', n) for line n of
    the original, ('patch', k) for line k (0-based) of the patch. Like the
    engine, but an error where the engine would change inserted lines or go
    past the end."""
    menu = [(t, ('orig', i + 1)) for i, t in enumerate(orig)]

    def find(n):
        for idx, entry in enumerate(menu):
            if entry[1] == ('orig', n):
                return idx
        raise PatchError('original line %d is gone (deleted or replaced before)' % n)

    for cmd, args, first, last in parse_hunks(plines):
        if cmd == 'ORI_HASH':
            continue
        n = int(args[0])
        if cmd == 'LINES_DELETE':
            count = int(args[1]) + 1 - n if len(args) >= 2 else 1
            idx = find(n)
            if idx + count > len(menu) or any(o[0] != 'orig' for t, o in menu[idx:idx + count]):
                raise PatchError('LINES_DELETE %s removes inserted lines' % ' '.join(args))
            del menu[idx:idx + count]
        elif cmd == 'LINES_INSERT':
            idx = find(n) + 1
            menu[idx:idx] = [(plines[k], ('patch', k)) for k in range(first + 1, last)]
        else:
            idx = find(n)
            for k in range(first + 1, last):
                if idx >= len(menu) or menu[idx][1][0] != 'orig':
                    raise PatchError('LINES_REPLACE %d changes inserted lines' % n)
                menu[idx] = (plines[k], ('patch', k))
                idx += 1
    return menu


class MenuPatch:
    """A patch and the menu it gives, edited through that menu."""

    def __init__(self, orig_lines, plines):
        self.orig = orig_lines
        self.plines = plines
        self.refresh()

    def refresh(self):
        self.out = apply(self.orig, self.plines)

    def text(self):
        return [t for t, o in self.out]

    def _hunk_of(self, k):
        for hunk in parse_hunks(self.plines):
            if hunk[2] < k < hunk[3]:
                return hunk
        raise PatchError('patch line %d is in no hunk' % (k + 1))

    def _add_hunk(self, lo, hi, lines):
        """Adds a hunk about original lines lo to hi, after every hunk about
        these lines: an INSERT after a line must come before what replaces or
        deletes the line, and the last INSERT after a line lands first, right
        after it. Otherwise the hunk goes before the first one about a later
        line, which keeps a sorted patch sorted."""
        hunks = [h for h in parse_hunks(self.plines) if h[0] != 'ORI_HASH']
        start = 0
        for cmd, args, first, last in hunks:
            n = int(args[0])
            end = n + last - first - 2 if cmd == 'LINES_REPLACE' else \
                int(args[1]) if cmd == 'LINES_DELETE' and len(args) > 1 else n
            if n <= hi and end >= lo:
                start = last + 1
        pos = len(self.plines)
        for cmd, args, first, last in hunks:
            if first >= start and int(args[0]) > hi:
                pos = first
                break
        block = lines + ['']
        if pos > 0 and self.plines[pos - 1].strip():
            block = [''] + block
        self.plines[pos:pos] = block

    def _split_replace(self, k, middle):
        """Splits the REPLACE hunk holding patch line k: its lines before k,
        then the hunks 'middle' builds from the original line k replaces, then
        its lines after k."""
        cmd, args, first, last = self._hunk_of(k)
        n = int(args[0])
        at = n + k - first - 1
        before, after = self.plines[first + 1:k], self.plines[k + 1:last]
        new = middle(at, self.plines[k])
        if before:
            new = ['LINES_REPLACE %d [' % n] + before + [']', ''] + new
        if after:
            new += ['', 'LINES_REPLACE %d [' % (at + 1)] + after + [']']
        self.plines[first:last + 1] = new

    def set(self, idx, new):
        """Replaces line idx of the patched menu."""
        t, o = self.out[idx]
        if o[0] == 'patch':
            self.plines[o[1]] = new
        else:
            self._add_hunk(o[1], o[1], ['LINES_REPLACE %d [' % o[1], new, ']'])
        self.refresh()

    def insert_after(self, idx, lines):
        """Inserts lines after line idx of the patched menu."""
        if idx < 0:
            raise PatchError('nothing can go before the first line of the menu')
        t, o = self.out[idx]
        if o[0] == 'orig':
            # an INSERT added later lands first, right after the line
            self._add_hunk(o[1], o[1], ['LINES_INSERT %d [' % o[1]] + lines + [']'])
        elif self._hunk_of(o[1])[0] == 'LINES_INSERT':
            self.plines[o[1] + 1:o[1] + 1] = lines
        else:
            # after a replaced line: an INSERT after that original line, which
            # must come before what replaces it (more lines in the REPLACE
            # would replace more original lines)
            self._split_replace(o[1], lambda at, text: ['LINES_INSERT %d [' % at] + lines + [']', '',
                                                        'LINES_REPLACE %d [' % at, text, ']'])
        self.refresh()

    def delete(self, first, last):
        """Deletes lines first to last of the patched menu."""
        idx = last
        while idx >= first:
            t, o = self.out[idx]
            if o[0] == 'orig':
                # a run of consecutive original lines, with nothing inserted
                # between them: one LINES_DELETE
                j = idx
                while j > first and self.out[j - 1][1] == ('orig', self.out[j][1][1] - 1):
                    j -= 1
                a, b = self.out[j][1][1], o[1]
                self._add_hunk(a, b, ['LINES_DELETE %d' % a if a == b else 'LINES_DELETE %d %d' % (a, b)])
                idx = j - 1
            else:
                k = o[1]
                cmd, args, hfirst, hlast = self._hunk_of(k)
                if cmd == 'LINES_INSERT':
                    if hlast - hfirst == 2:
                        # the hunk goes, with a blank line around it
                        end = hlast + 1
                        if end < len(self.plines) and not self.plines[end].strip() and \
                                (hfirst == 0 or not self.plines[hfirst - 1].strip()):
                            end += 1
                        del self.plines[hfirst:end]
                    else:
                        del self.plines[k]
                else:
                    self._split_replace(k, lambda at, text: ['LINES_DELETE %d' % at])
                idx -= 1
            self.refresh()

    def update(self, new):
        """Edits the patch so that the patched menu is 'new' (a list of lines)."""
        ops = difflib.SequenceMatcher(None, self.text(), new, autojunk=False).get_opcodes()
        for tag, i1, i2, j1, j2 in reversed(ops):
            if tag == 'equal':
                continue
            common = min(i2 - i1, j2 - j1)
            for k in range(common):
                self.set(i1 + k, new[j1 + k])
            if j2 - j1 > common:
                self.insert_after(i1 + common - 1, new[j1 + common:j2])
            if i2 - i1 > common:
                self.delete(i1 + common, i2 - 1)
        if self.text() != new:
            raise PatchError('internal error: the updated patch gives another menu')


def read_text(path):
    with open(path, 'rb') as f:
        return f.read().decode('latin-1')


def ori_hash(plines):
    for cmd, args, first, last in parse_hunks(plines):
        if cmd == 'ORI_HASH':
            return int(args[0], 0)
    raise PatchError('no ORI_HASH')


def original_menu(patch_path, plines, menu, base):
    """The original menu text, checked against ORI_HASH."""
    crc = ori_hash(plines)
    if menu:
        with open(menu, 'rb') as f:
            data = f.read()
        if zlib.crc32(data) & 0xffffffff != crc:
            raise PatchError('%s has the CRC32 %d, the patch is for %d'
                             % (menu, zlib.crc32(data) & 0xffffffff, crc))
        return data.decode('latin-1')
    base = base or os.environ.get('JK2_BASE')
    if not base:
        raise PatchError('give the original menu (--menu) or the base folder of JK2 (--base or JK2_BASE)')
    name = os.path.basename(patch_path)
    if not name.endswith('_patch'):
        raise PatchError('%s: a patch is named <menu>_patch' % patch_path)
    name = name[:-len('_patch')].lower()
    for pk3 in sorted(f for f in os.listdir(base) if f.lower().endswith('.pk3')):
        with zipfile.ZipFile(os.path.join(base, pk3)) as z:
            for info in z.infolist():
                if os.path.basename(info.filename).lower() == name and info.CRC == crc:
                    print('%s: %s in %s' % (os.path.basename(patch_path), info.filename, pk3), file=sys.stderr)
                    return z.read(info).decode('latin-1')
    raise PatchError('no %s with the CRC32 %d in the pk3s of %s' % (name, crc, base))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command')
    show = sub.add_parser('show', help='write the patched menu')
    show.add_argument('patch')
    show.add_argument('-o', '--output')
    update = sub.add_parser('update', help='rewrite the patch from an edited menu')
    update.add_argument('patch')
    update.add_argument('edited')
    for p in (show, update):
        p.add_argument('--base', help='base folder of JK2, with the retail pk3s')
        p.add_argument('--menu', help='the original menu file')
    args = parser.parse_args()
    if not args.command:
        print(__doc__)
        return 2

    try:
        ptext = read_text(args.patch)
        plines = lineize(ptext)
        orig = lineize(original_menu(args.patch, plines, args.menu, args.base))
        mp = MenuPatch(orig, plines)
        if args.command == 'show':
            data = ''.join(t + '\n' for t in mp.text())
            if args.output:
                with open(args.output, 'wb') as f:
                    f.write(data.encode('latin-1'))
            else:
                sys.stdout.write(data)
        else:
            mp.update(lineize(read_text(args.edited)))
            newline = '\r\n' if '\r\n' in ptext else '\n'
            with open(args.patch, 'wb') as f:
                f.write(''.join(l + newline for l in mp.plines).encode('latin-1'))
            print('%s: %d lines' % (args.patch, len(mp.plines)), file=sys.stderr)
    except PatchError as e:
        print('error: %s' % e, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
