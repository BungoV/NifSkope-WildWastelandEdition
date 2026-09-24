#!/usr/bin/env python3
"""Splice lane LODUI1's self-test block into WW_LODGEN_TEST.

An exact-once anchor carrying the file's real line ending (src/ is LF-only),
a --check that writes nothing, and a CR assert either side, per the
ww-anchored-hookup skill.  The block itself is written by the Write tool into
selftest_block.cpp so no heredoc can halve a backslash in it.
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TARGET = os.path.join(ROOT, 'src', 'nifskope_ui.cpp')
BLOCK = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'selftest_block.cpp')

ANCHOR = (
    "\t\t\t\t\t\t} else {\n"
    "\t\t\t\t\t\t\tcheck( \"the target switches the outputs\", false );\n"
    "\t\t\t\t\t\t}\n"
)
MARKER = "LANE LODUI1, 2026-09-11"


def main():
    apply_ = '--apply' in sys.argv
    raw = open(TARGET, 'rb').read()
    text = raw.decode('utf-8')
    block = open(BLOCK, 'rb').read().decode('utf-8')
    print('target %d bytes, CR %d, LF %d' % (len(raw), raw.count(b'\r'), raw.count(b'\n')))
    print('anchor matches: %d' % text.count(ANCHOR))
    print('marker already present: %d' % text.count(MARKER))
    if text.count(ANCHOR) != 1:
        print('REFUSED: the anchor must match exactly once')
        return 2
    if text.count(MARKER):
        print('REFUSED: the block is already in')
        return 3
    if not apply_:
        print('--check only, nothing written')
        return 0
    out = text.replace(ANCHOR, ANCHOR + block)
    data = out.encode('utf-8')
    assert data.count(b'\r') == raw.count(b'\r'), 'CR count moved'
    open(TARGET, 'wb').write(data)
    print('applied: %d -> %d bytes, CR %d' % (len(raw), len(data), data.count(b'\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
