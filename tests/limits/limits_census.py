#!/usr/bin/env python3
"""Census of the client and entity limit symbols in the engine sources.

Step 1 of docs/PROTOCOL-1.05.md replaced the uses of MAX_CLIENTS and
MAX_GENTITIES that mean "the limit of the protocol in use" with the accessors
of src/qcommon/mv_limits.h. Every use left must be listed, with its class, in
allowlist.txt next to this script:

  def        the definition of the symbol
  capacity   an array size, a clear or a sweep of a whole array, a pool guard
  wire       the protocol 15/16 encoding (step 2 makes it per protocol)
  cs-layout  the 1.02-1.04 configstring layout (step 2: a per-protocol table)
  abi        a layout shared with the game modules
  dead       code no target compiles or calls

A use that is not listed fails (convert it to an accessor or classify it), and
so does a listed use that is gone (remove its line). Comments are ignored.

Only the client and entity symbols are counted. The sites that depend on
MAX_MODELS, MAX_SOUNDS, MAX_CONFIGSTRINGS, MAX_GAMESTATE_CHARS or the
configstring indices (CS_PLAYERS...) are marked by the static_asserts of
MV_ASSERT_LEGACY_MODEL_ENCODING() and MV_ASSERT_LEGACY_CONFIGSTRINGS()
(src/qcommon/mv_limits.h) instead.

usage: limits_census.py <repository root> [--print]
  --print  write the current uses in the allowlist format, '?' as the class
           of the unlisted ones, instead of checking
"""

import collections
import os
import re
import sys

SYMBOLS = re.compile(r'\b(MAX_CLIENTS|MAX_GENTITIES|GENTITYNUM_BITS|ENTITYNUM_NONE|ENTITYNUM_WORLD|'
                     r'ENTITYNUM_MAX_NORMAL|CS_CHARSKINS|CS_LOCATIONS|CS_PARTICLES|CS_EFFECTS|'
                     r'CS_LIGHT_STYLES|CS_STRING_PACKAGES|CS_MAX)\b')
ACCESSORS = re.compile(r'\b(MV_MaxClients|MV_MaxGEntities|MV_ENTITYNUM_NONE|MV_ENTITYNUM_WORLD|MV_CurrentLimits)\b')
CLASSES = ('def', 'capacity', 'wire', 'cs-layout', 'abi', 'dead')
SKIP = ('src/mvsdk/', 'src/qcommon/mv_limits.h')


def code_lines(text):
    """The lines of a C or C++ file without their comments (strings kept)."""
    out, cur, i, n = [], [], 0, len(text)
    in_block = False
    while i < n:
        c = text[i]
        if in_block:
            if text.startswith('*/', i):
                in_block, i = False, i + 2
                continue
            if c == '\n':
                out.append(''.join(cur))
                cur = []
            i += 1
        elif text.startswith('/*', i):
            in_block, i = True, i + 2
        elif text.startswith('//', i):
            while i < n and text[i] != '\n':
                i += 1
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            cur.append(text[i:j + 1])
            i = j + 1
        elif c == '\n':
            out.append(''.join(cur))
            cur, i = [], i + 1
        else:
            cur.append(c)
            i += 1
    out.append(''.join(cur))
    return out


def collapse(line):
    return ' '.join(line.split())


def scan(root):
    uses, misplaced = [], []
    for base, dirs, files in os.walk(os.path.join(root, 'src')):
        dirs.sort()
        for name in sorted(files):
            if not name.endswith(('.c', '.cpp', '.h')):
                continue
            path = os.path.relpath(os.path.join(base, name), root).replace(os.sep, '/')
            if path.startswith(SKIP):
                continue
            with open(os.path.join(root, path), encoding='latin-1') as f:
                lines = code_lines(f.read())
            for number, line in enumerate(lines, 1):
                if SYMBOLS.search(line):
                    uses.append((path, collapse(line), number))
                # every accessor of the line, not only the first one
                accessors = set(ACCESSORS.findall(line))
                if accessors and (path.startswith('src/renderer/') or
                                  ('MV_CurrentLimits' in accessors and path != 'src/qcommon/common.cpp')):
                    misplaced.append('%s:%d: %s' % (path, number, collapse(line)))
    return uses, misplaced


def read_allowlist(path):
    entries = collections.Counter()
    classes = {}
    with open(path, encoding='latin-1') as f:
        for n, raw in enumerate(f, 1):
            raw = raw.strip()
            if not raw or raw.startswith('#'):
                continue
            kind, file, line = raw.split(' ', 2)
            if kind not in CLASSES:
                sys.exit('%s:%d: unknown class %r' % (path, n, kind))
            entries[(file, line)] += 1
            classes[(file, line)] = kind
    return entries, classes


def main():
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != '--print'):
        sys.exit(__doc__)
    root = sys.argv[1]
    allowlist = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'allowlist.txt')
    uses, misplaced = scan(root)
    entries, classes = read_allowlist(allowlist) if os.path.exists(allowlist) else (collections.Counter(), {})

    if len(sys.argv) == 3:
        for path, line, number in uses:
            print('%s %s %s' % (classes.get((path, line), '?'), path, line))
        return

    found = collections.Counter((path, line) for path, line, number in uses)
    errors = []
    for path, line, number in uses:
        if found[(path, line)] > entries[(path, line)]:
            errors.append('%s:%d: not in the allowlist: %s' % (path, number, line))
    for key, count in sorted(entries.items()):
        if count > found[key]:
            errors.append('allowlist: gone from the sources: %s %s' % key)
    errors += ['%s: limit accessor out of place' % m for m in misplaced]
    if errors:
        print('\n'.join(sorted(set(errors))))
        print('limits census FAILED: convert the use to MV_MaxClients(), MV_MaxGEntities(), '
              'MV_ENTITYNUM_NONE or MV_ENTITYNUM_WORLD, or classify it in %s' % allowlist)
        sys.exit(1)
    print('limits census passed: %d classified uses' % len(uses))


if __name__ == '__main__':
    main()
