#!/usr/bin/env python3
"""Generate a GPL-clean stub game directory for the smoke tests.

The retail JK2 assets can't be used in CI, so this writes the minimum the
engine needs to start a server and a client: an assets5.pk3 placeholder,
empty string packages, a bot list, empty menu lists and a tiny box map
(maps/ci_box.bsp). The mvsdk assets built with the engine (assetsmv.pk3,
assetsmv2.pk3) are copied from the build output.

usage: make_stub_base.py <output dir> <directory holding the built base/>
"""

import os
import shutil
import struct
import sys
import zipfile


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = 'wb' if isinstance(data, bytes) else 'w'
    with open(path, mode) as f:
        f.write(data)


def make_box_bsp(path):
    """Minimal RBSP v1 map: one solid floor brush, one node, two leafs."""
    size = 1024.0
    ents = (
        '{\n"classname" "worldspawn"\n}\n'
        '{\n"classname" "info_player_deathmatch"\n"origin" "0 0 64"\n"angle" "0"\n}\n'
        '{\n"classname" "info_player_deathmatch"\n"origin" "256 0 64"\n"angle" "180"\n}\n'
    ).encode() + b'\0'
    while len(ents) % 4:
        ents += b'\0'

    shaders = struct.pack('64sii', b'textures/common/caulk', 0, 1)  # CONTENTS_SOLID

    planes_def = [
        ((0, 0, 1), 0), ((0, 0, -1), 0),            # floor top
        ((0, 0, -1), 64), ((0, 0, 1), -64),         # floor bottom (z=-64)
        ((1, 0, 0), size), ((-1, 0, 0), -size),     # +x
        ((-1, 0, 0), size), ((1, 0, 0), -size),     # -x
        ((0, 1, 0), size), ((0, -1, 0), -size),     # +y
        ((0, -1, 0), size), ((0, 1, 0), -size),     # -y
    ]
    planes = b''.join(struct.pack('4f', n[0], n[1], n[2], d) for n, d in planes_def)

    b = int(size)
    nodes = struct.pack('3i3i3i', 0, -1, -2, -b, -b, -64, b, b, b)
    leafs = (struct.pack('2i3i3i4i', 0, 0, -b, -b, 0, b, b, b, 0, 0, 0, 1) +
             struct.pack('2i3i3i4i', 0, 0, -b, -b, -64, b, b, 0, 0, 0, 1, 1))
    leafbrushes = struct.pack('2i', 0, 0)
    models = struct.pack('6f4i', -size, -size, -64, size, size, size, 0, 0, 0, 1)
    brushes = struct.pack('3i', 0, 6, 0)
    brushsides = b''.join(struct.pack('3i', p, 0, 0) for p in (0, 2, 4, 6, 8, 10))

    lumps = [b''] * 18
    lumps[0] = ents
    lumps[1] = shaders
    lumps[2] = planes
    lumps[3] = nodes
    lumps[4] = leafs
    lumps[6] = leafbrushes
    lumps[7] = models
    lumps[8] = brushes
    lumps[9] = brushsides

    ofs = 8 + 18 * 8
    directory = b''
    body = b''
    for lump in lumps:
        directory += struct.pack('2i', ofs, len(lump))
        body += lump
        ofs += len(lump)
    write(path, b'RBSP' + struct.pack('i', 1) + directory + body)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    out, built = sys.argv[1], sys.argv[2]
    base = os.path.join(out, 'base')
    if os.path.isdir(out):
        shutil.rmtree(out)

    # FS_Startup only checks that assets5.pk3 exists. The "white" shader of the
    # retail scripts is used for every colored 2D fill; without it the
    # implicit shader ignores the color.
    os.makedirs(base)
    with zipfile.ZipFile(os.path.join(base, 'assets5.pk3'), 'w') as z:
        z.writestr('stub.txt', 'jk2mv smoke test placeholder\n')
        z.writestr('shaders/stub.shader',
                   'white\n{\n\t{\n\t\tmap $whiteimage\n\t\tblendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n'
                   '\t\trgbgen vertex\n\t\talphagen vertex\n\t}\n}\n')

    write(os.path.join(base, 'mpdefault.cfg'), '// smoke test stub\n')

    # string packages need a unique ID each
    for i, name in enumerate(('CON_TEXT', 'MP_INGAME', 'MP_SVGAME', 'SP_INGAME', 'STR_SERVER')):
        write(os.path.join(base, 'strip', name + '.sp'),
              'VERSION 1\nID %d\nREFERENCE %s\nCOUNT 0\n' % (200 + i, name))

    bots = ''.join('{\nname "B%02d"\nmodel "kyle"\npersonality "botfiles/default.jkb"\n}\n' % i
                   for i in range(1, 32))
    write(os.path.join(base, 'botfiles', 'bots.txt'), bots)

    # the menu module fails with a recursive error without menu lists
    for name in ('menus.txt', 'jk2mpmenus.txt'):
        write(os.path.join(base, 'ui', name), '{\n}\n')

    make_box_bsp(os.path.join(base, 'maps', 'ci_box.bsp'))

    for name in ('assetsmv.pk3', 'assetsmv2.pk3'):
        src = os.path.join(built, 'base', name)
        if not os.path.isfile(src):
            sys.exit('missing %s, build the engine first' % src)
        shutil.copy(src, base)

    print('stub game directory written to', out)


if __name__ == '__main__':
    main()
