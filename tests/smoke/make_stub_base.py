#!/usr/bin/env python3
"""Generate a GPL-clean stub game directory for the smoke tests.

The retail JK2 assets can't be used in CI, so this writes the minimum the
engine needs to start a server and a client: an assets5.pk3 placeholder,
empty string packages, a bot list, a menu list with a stand-in full screen
main menu and a tiny box map (maps/ci_box.bsp). The mvsdk assets built with
the engine (assetsmv.pk3, assetsmv2.pk3) are copied from the build output.

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


MST_PLANAR = 1
LIGHTMAP_BY_VERTEX = -3
LS_NORMAL, LS_NONE = 0, 0xff

# visible surfaces for the renderer test (testscene ci_box -256 0 96):
# shader, corners, vertex color. A room, a wall, a white lamp and two
# overlapping red strips; the additive ones go over 1.0 in HDR and glow.
SCENE_SURFACES = [
    ('textures/ci/floor', [(-512, -512, 0), (512, -512, 0), (512, 512, 0), (-512, 512, 0)], (70, 70, 80)),
    ('textures/ci/wall', [(448, -384, 0), (448, 384, 0), (448, 384, 384), (448, -384, 384)], (110, 110, 120)),
    # dark room around it, so every pixel is drawn
    ('textures/ci/wall', [(-512, -512, 512), (512, -512, 512), (512, 512, 512), (-512, 512, 512)], (20, 24, 40)),
    ('textures/ci/wall', [(512, -512, 0), (512, 512, 0), (512, 512, 512), (512, -512, 512)], (30, 34, 50)),
    ('textures/ci/wall', [(-512, -512, 0), (-512, 512, 0), (-512, 512, 512), (-512, -512, 512)], (30, 34, 50)),
    ('textures/ci/wall', [(-512, 512, 0), (512, 512, 0), (512, 512, 512), (-512, 512, 512)], (36, 40, 56)),
    ('textures/ci/wall', [(-512, -512, 0), (512, -512, 0), (512, -512, 512), (-512, -512, 512)], (36, 40, 56)),
    ('textures/ci/lamp', [(440, -48, 160), (440, 48, 160), (440, 48, 256), (440, -48, 256)], (255, 255, 255)),
    ('textures/ci/red', [(436, 160, 64), (436, 192, 64), (436, 192, 320), (436, 160, 320)], (255, 255, 255)),
    ('textures/ci/red', [(432, 164, 64), (432, 196, 64), (432, 196, 320), (432, 164, 320)], (255, 255, 255)),
]

# Full screen like the retail main menu, so the tests go through the same
# UIMENU_MAIN path: what it covers isn't drawn at all. Pure green where the
# 3D view would be (render_smoke.py's MENU_PROBE), with blue and red bands
# for client_smoke.py's blank screenshot check. No text: the retail fonts
# aren't there.
MAIN_MENU = '''{
	menuDef
	{
		name		"main"
		fullscreen	1
		rect		0 0 640 480
		visible		1
		style		1
		background	"white"
		backcolor	0 1 0 1
		itemDef
		{
			name		"ci_top"
			rect		0 0 640 40
			style		1
			backcolor	0 0 1 1
			visible		1
			decoration
		}
		itemDef
		{
			name		"ci_bottom"
			rect		0 440 640 40
			style		1
			backcolor	1 0 0 1
			visible		1
			decoration
		}
	}
}
'''

SCENE_SHADERS = (
    'textures/ci/floor\n{\n\tcull none\n\t{\n\t\tmap $whiteimage\n\t\trgbGen vertex\n\t}\n}\n'
    'textures/ci/wall\n{\n\tcull none\n\t{\n\t\tmap $whiteimage\n\t\trgbGen vertex\n\t}\n}\n'
    'textures/ci/lamp\n{\n\tcull none\n\t{\n\t\tmap $whiteimage\n\t\tblendfunc GL_ONE GL_ONE\n'
    '\t\trgbGen const ( 1 1 1 )\n\t\tglow\n\t}\n}\n'
    'textures/ci/red\n{\n\tcull none\n\t{\n\t\tmap $whiteimage\n\t\tblendfunc GL_ONE GL_ONE\n'
    '\t\trgbGen const ( 1 0.25 0.1 )\n\t\tglow\n\t}\n}\n'
)


def scene_lumps(first_shader):
    """Shaders, vertexes, indexes and surfaces of SCENE_SURFACES."""
    names = []
    verts = indexes = surfaces = b''
    for i, (shader, corners, rgb) in enumerate(SCENE_SURFACES):
        if shader not in names:
            names.append(shader)
        e1 = [corners[1][k] - corners[0][k] for k in range(3)]
        e2 = [corners[2][k] - corners[0][k] for k in range(3)]
        normal = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
        length = sum(c * c for c in normal) ** 0.5
        normal = [c / length for c in normal]
        # facing into the room, as a compiler would write them (lighting uses them)
        to_center = [(0, 0, 200)[k] - corners[0][k] for k in range(3)]
        if sum(normal[k] * to_center[k] for k in range(3)) < 0:
            normal = [-c for c in normal]
        for j, xyz in enumerate(corners):
            verts += struct.pack('3f2f8f3f16B', *xyz, j in (1, 2), j in (2, 3), *([0.0] * 8), *normal,
                                 *rgb, 255, *([0] * 12))
        indexes += struct.pack('6i', 0, 1, 2, 0, 2, 3)
        surfaces += struct.pack('3i4i8B4i4i4i2i3f9f2i', first_shader + names.index(shader), -1, MST_PLANAR,
                                i * 4, 4, i * 6, 6,
                                LS_NONE, LS_NONE, LS_NONE, LS_NONE, LS_NORMAL, LS_NONE, LS_NONE, LS_NONE,
                                LIGHTMAP_BY_VERTEX, LIGHTMAP_BY_VERTEX, LIGHTMAP_BY_VERTEX, LIGHTMAP_BY_VERTEX,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, *normal, 0, 0)
    shaders = b''.join(struct.pack('64sii', n.encode(), 0, 0) for n in names)
    return shaders, verts, indexes, surfaces


def make_box_bsp(path):
    """Minimal RBSP v1 map: one solid floor brush, one node, two leafs, and
    the visible SCENE_SURFACES in the open leaf."""
    size = 1024.0
    ents = (
        '{\n"classname" "worldspawn"\n}\n'
        '{\n"classname" "info_player_deathmatch"\n"origin" "0 0 64"\n"angle" "0"\n}\n'
        '{\n"classname" "info_player_deathmatch"\n"origin" "256 0 64"\n"angle" "180"\n}\n'
    ).encode() + b'\0'
    while len(ents) % 4:
        ents += b'\0'

    shaders = struct.pack('64sii', b'textures/common/caulk', 0, 1)  # CONTENTS_SOLID
    scene_shaders, verts, indexes, surfaces = scene_lumps(1)
    shaders += scene_shaders
    num_surfaces = len(SCENE_SURFACES)

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
    # the solid leaf is cluster -1, as q3map writes it
    leafs = (struct.pack('2i3i3i4i', 0, 0, -b, -b, 0, b, b, b, 0, num_surfaces, 0, 1) +
             struct.pack('2i3i3i4i', -1, 0, -b, -b, -64, b, b, 0, 0, 0, 1, 1))
    leafbrushes = struct.pack('2i', 0, 0)
    leafsurfaces = struct.pack('%di' % num_surfaces, *range(num_surfaces))
    models = struct.pack('6f4i', -size, -size, -64, size, size, size, 0, num_surfaces, 0, 1)
    brushes = struct.pack('3i', 0, 6, 0)
    # axial sides in q3map's order, -x +x -y +y -z +z: CM_BoundBrush takes
    # the brush bounds from them
    brushsides = b''.join(struct.pack('3i', p, 0, 0) for p in (6, 4, 10, 8, 2, 0))

    lumps = [b''] * 18
    lumps[0] = ents
    lumps[1] = shaders
    lumps[2] = planes
    lumps[3] = nodes
    lumps[4] = leafs
    lumps[5] = leafsurfaces
    lumps[6] = leafbrushes
    lumps[7] = models
    lumps[8] = brushes
    lumps[9] = brushsides
    lumps[10] = verts
    lumps[11] = indexes
    lumps[13] = surfaces

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
                   '\t\trgbgen vertex\n\t\talphagen vertex\n\t}\n}\n' + SCENE_SHADERS)

    write(os.path.join(base, 'mpdefault.cfg'), '// smoke test stub\n')

    # string packages need a unique ID each
    for i, name in enumerate(('CON_TEXT', 'MP_INGAME', 'MP_SVGAME', 'SP_INGAME', 'STR_SERVER')):
        write(os.path.join(base, 'strip', name + '.sp'),
              'VERSION 1\nID %d\nREFERENCE %s\nCOUNT 0\n' % (200 + i, name))

    bots = ''.join('{\nname "B%02d"\nmodel "kyle"\npersonality "botfiles/default.jkb"\n}\n' % i
                   for i in range(1, 32))
    write(os.path.join(base, 'botfiles', 'bots.txt'), bots)

    # the menu module fails with a recursive error without menu lists
    write(os.path.join(base, 'ui', 'ci_main.menu'), MAIN_MENU)
    for name in ('menus.txt', 'jk2mpmenus.txt'):
        write(os.path.join(base, 'ui', name), '{\n\tloadMenu { "ui/ci_main.menu" }\n}\n')

    make_box_bsp(os.path.join(base, 'maps', 'ci_box.bsp'))

    for name in ('assetsmv.pk3', 'assetsmv2.pk3'):
        src = os.path.join(built, 'base', name)
        if not os.path.isfile(src):
            sys.exit('missing %s, build the engine first' % src)
        shutil.copy(src, base)

    print('stub game directory written to', out)


if __name__ == '__main__':
    main()
