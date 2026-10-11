#!/usr/bin/env python3
"""Self-test of limits_census.py: its scan on small trees written for it.

The census (ctest limits_census) only ever sees the real sources, which keep
its rules, so a rule it stops enforcing would go unnoticed there. Here each
case plants one use and checks what scan() reports.

usage: test_limits_census.py
"""

import os
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True	# no __pycache__ in the source tree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import limits_census  # noqa: E402


def scan(files):
    """limits_census.scan() on a tree holding only `files` ({path: text})."""
    with tempfile.TemporaryDirectory() as root:
        for path, text in files.items():
            full = os.path.join(root, path)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, 'w', encoding='latin-1') as f:
                f.write(text)
        return limits_census.scan(root)


class AccessorPlace(unittest.TestCase):
    """MV_CurrentLimits() belongs to common.cpp, no accessor to the renderer."""

    def assertMisplaced(self, path, line):
        uses, misplaced = scan({path: line + '\n'})
        self.assertEqual(misplaced, ['%s:1: %s' % (path, line)])

    def assertInPlace(self, path, line):
        uses, misplaced = scan({path: line + '\n'})
        self.assertEqual(misplaced, [])

    def test_current_limits_outside_common(self):
        self.assertMisplaced('src/server/sv_a.cpp',
                             'static int f( void ) { return MV_CurrentLimits()->gentityBits; }')

    # an accessor after another one on the same line is still checked
    def test_current_limits_after_another_accessor(self):
        self.assertMisplaced('src/server/sv_a.cpp',
                             'static int f( void ) { return MV_MaxClients() + MV_CurrentLimits()->gentityBits; }')
        self.assertMisplaced('src/client/cl_a.cpp',
                             'int n = MV_ENTITYNUM_NONE - MV_CurrentLimits()->maxGEntities;')

    def test_current_limits_in_common(self):
        self.assertInPlace('src/qcommon/common.cpp',
                           'const mvprotocolLimits_t *MV_CurrentLimits( void ) { return MV_MaxClients() ? 0 : 0; }')

    def test_accessors_outside_the_renderer(self):
        self.assertInPlace('src/server/sv_a.cpp',
                           'if ( num < 0 || num >= MV_MaxGEntities() || num == MV_ENTITYNUM_WORLD ) {')

    def test_any_accessor_in_the_renderer(self):
        for accessor in ('MV_MaxClients()', 'MV_MaxGEntities()', 'MV_ENTITYNUM_NONE', 'MV_ENTITYNUM_WORLD',
                         'MV_CurrentLimits()'):
            self.assertMisplaced('src/renderer/tr_a.cpp', 'int n = 1 + %s;' % accessor)
        self.assertMisplaced('src/renderer/tr_a.cpp', 'int n = REFENTITYNUM_WORLD + MV_MaxClients();')

    def test_comments_are_ignored(self):
        self.assertInPlace('src/server/sv_a.cpp', 'int n; // MV_CurrentLimits()')
        self.assertInPlace('src/renderer/tr_a.cpp', 'int n; /* MV_MaxClients() */')

    def test_mv_limits_h_is_skipped(self):
        self.assertInPlace('src/qcommon/mv_limits.h',
                           'static inline int MV_MaxClients( void ) { return MV_CurrentLimits()->maxClients; }')


class SymbolUses(unittest.TestCase):
    """Every use of a limit symbol outside comments is counted, with its line."""

    def test_uses(self):
        uses, misplaced = scan({
            'src/server/sv_a.cpp': 'client_t c[MAX_CLIENTS];\n'
                                   '// MAX_GENTITIES in a comment\n'
                                   'int  e  =  ENTITYNUM_NONE;\n',
            'src/mvsdk/code/game/g_a.c': 'int x[MAX_CLIENTS];\n',
        })
        self.assertEqual(uses, [('src/server/sv_a.cpp', 'client_t c[MAX_CLIENTS];', 1),
                                ('src/server/sv_a.cpp', 'int e = ENTITYNUM_NONE;', 3)])
        self.assertEqual(misplaced, [])


if __name__ == '__main__':
    unittest.main()
