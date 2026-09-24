# -*- coding: utf-8 -*-
"""gatefix4.py -- lane BUILD5b: WW_WATER_MARK_BODY, so the picture can be taken
at a framing that already exists.

The self-test's case is "the largest river", which on the Commonwealth is body
2 (29,312 texels, cells -9..2 / -38..-22).  The before-picture this lane owes a
pair for is lane WATER2's `charles_flow.png`, rendered at cells
[-16,-21]..[-6,-4] -- which is body 3's bbox exactly, the Charles.  The gate
must keep choosing the largest river on any worldspace (that is what makes it
portable), so the named body is an OVERRIDE and nothing else changes.

    python scratchpad/water3_20260910/gatefix4.py --check
    python scratchpad/water3_20260910/gatefix4.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv

ANCHOR = b'\tcheck( QStringLiteral( "the file offers a river and a second body to mark" ),\n'
NEW = (
    b'\t/* WW_WATER_MARK_BODY=<id> marks a NAMED body instead of the largest\n'
    b'\t * river.  The case itself stays "the largest river", because that is\n'
    b'\t * what makes this harness run on any worldspace; this exists so a\n'
    b'\t * before/after pair can be taken at a framing somebody already has --\n'
    b'\t * the Charles is body 3 on the Commonwealth and lane WATER2\'s flow\n'
    b'\t * renders are cut to its cells.  The neighbour is re-picked so it can\n'
    b'\t * never be the body under test. */\n'
    b'\t{\n'
    b'\t\tconst QByteArray pin = qgetenv( "WW_WATER_MARK_BODY" );\n'
    b'\t\tbool okPin = false;\n'
    b'\t\tconst int want = pin.toInt( &okPin );\n'
    b'\t\tLodtWaterBody pb;\n'
    b'\t\tif ( okPin && want >= 1 && doc.body( want, pb ) && pb.area > 0 ) {\n'
    b'\t\t\triver = want;\n'
    b'\t\t\tneighbour = 0;\n'
    b'\t\t\tbestOther = 0;\n'
    b'\t\t\tfor ( int i = 1; i <= doc.bodyCount(); i++ ) {\n'
    b'\t\t\t\tLodtWaterBody b;\n'
    b'\t\t\t\tdoc.body( i, b );\n'
    b'\t\t\t\tif ( i != river && b.cls != 0 && b.area > bestOther ) {\n'
    b'\t\t\t\t\tbestOther = b.area;\n'
    b'\t\t\t\t\tneighbour = i;\n'
    b'\t\t\t\t}\n'
    b'\t\t\t}\n'
    b'\t\t\tsay( QStringLiteral( "WW_WATER_MARK_BODY=%1: marking that body instead of the "\n'
    b'\t\t\t\t"largest river" ).arg( river ) );\n'
    b'\t\t}\n'
    b'\t}\n'
) + ANCHOR

EDITS = [('src/watermark.cpp', [(ANCHOR, NEW)])]

bad = 0
for rel, edits in EDITS:
    path = os.path.join(ROOT, rel)
    with open(path, 'rb') as f:
        data = f.read()
    print('%-26s %s %8d bytes %6d lines CR=%d' % (
        rel, hashlib.sha1(data).hexdigest()[:16], len(data),
        data.count(b'\n'), data.count(b'\r')))
    cr0 = data.count(b'\r')
    hurt = False
    for anchor, new in edits:
        n = data.count(anchor)
        if n != 1:
            print('  REFUSED: %d matches for %r' % (n, anchor[:70]))
            bad += 1
            hurt = True
            continue
        before = len(data)
        data = data.replace(anchor, new, 1)
        print('  ok: %d -> %d bytes  (+%d)' % (before, len(data), len(data) - before))
    if data.count(b'\r') != cr0:
        print('  REFUSED: CR moved %d -> %d' % (cr0, data.count(b'\r')))
        bad += 1
        hurt = True
    if not check_only and not hurt:
        with open(path, 'wb') as f:
            f.write(data)
        print('  written: %d bytes %d lines CR=%d' % (
            len(data), data.count(b'\n'), data.count(b'\r')))

if bad:
    print('REFUSED (%d)' % bad)
    sys.exit(1)
if check_only:
    print('--check: nothing written')
sys.exit(0)
