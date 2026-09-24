#!/usr/bin/env python3
"""Byte identity across the FO4CSLOD move (lane LAYOUT1, 2026-09-16).

bungo, 2026-09-16 19:3x: "The folder should be called FO4CSLOD maybe, so it'd
be Data/FO4CSLOD, sound fine?".  Every FO4CS-target output moved under one
root that day.  The claim the move has to earn is narrow and exact:

    THE ONLY BYTES THAT CHANGED ARE THE GAME-RELATIVE PATH STRINGS WRITTEN
    INSIDE THE FILES, AND THE LENGTH WORDS THOSE STRINGS FORCE.

So this reads a pair of files -- one from an exe built before the move, one
from after -- rewrites the OLD file's path strings to the NEW spelling, repacks
any LODM envelope whose payload the rewrite resized (magic 'LODM', version,
payload length: no checksum, so the repack is exact and nothing else in the
envelope depends on the length), and then requires BYTE EQUALITY.

Run it with --no-rewrite and it is its own refuter: without the rewrite the
same pair MUST differ, and differ inside the path bytes.  A gate that cannot
fail proves nothing (CONSTITUTION 9), and "the paths changed" is only a fact
once the run that does not change them is seen to fail.

  lodgen_layout_diff.py --ws Commonwealth --pair OLD NEW [--pair ...]
                        [--no-rewrite] [--quiet]

Exit 0 when every pair is equal after the rewrite (or, with --no-rewrite, when
every pair that carries a path string DIFFERS -- the refuter's own success).
"""

import argparse
import os
import struct
import sys

BS = chr(92)


def spellings(ws):
    """(old, new) game-relative spellings, longest first so a prefix of one
    cannot eat another.  Both the raw form and the JSON form (where every
    backslash is doubled) are rewritten, because the same string is written
    into a .lodm payload as JSON text and into a manifest row as plain text."""
    raw = [
        # the impostor cards: per TREE, so they sit at the root, not under a ws
        ('Textures' + BS + 'Lodgen' + BS + 'Cards', 'FO4CSLOD' + BS + 'Cards'),
        # the aggregate sets: per worldspace, under that worldspace's folder
        ('Textures' + BS + 'Lodgen' + BS + 'Aggregate' + BS + ws,
         'FO4CSLOD' + BS + ws + BS + 'Aggregate'),
        # the object texture sets (mesh arrays, atlas, card arrays)
        ('Textures' + BS + 'Terrain' + BS + ws + BS + 'Objects',
         'FO4CSLOD' + BS + ws + BS + 'Objects'),
        # the terrain virtual texture's level containers, named by its index
        ('Terrain' + BS + ws + '.VT', 'FO4CSLOD' + BS + ws + BS + ws + '.VT'),
        # the landscape file and the native pair, where anything names them
        ('Terrain' + BS + ws + '.lod', 'FO4CSLOD' + BS + ws + BS + ws + '.lod'),
    ]
    out = []
    for old, new in raw:
        for o, n in ((old, new), (old.replace(BS, BS + BS), new.replace(BS, BS + BS))):
            out.append((o.encode('utf-8'), n.encode('utf-8')))
            # the writers spell `data\...` in lower case in some strings and
            # `Data\...` in others; the tail after the drive word is what moves
            lo, ln = o.lower().encode('utf-8'), n.encode('utf-8')
            if lo != o.encode('utf-8'):
                out.append((lo, ln))
    # longest first
    out.sort(key=lambda p: -len(p[0]))
    return out


def rewrite(data, subs):
    hits = []
    for old, new in subs:
        n = data.count(old)
        if n:
            data = data.replace(old, new)
            hits.append((old.decode('utf-8', 'replace'), n))
    return data, hits


def repack_lodm(data, original):
    """A LODM envelope carries its payload length at offset 8.  When the
    rewrite resizes the payload that word is a CONSEQUENCE of the path string,
    not a second change, so it is repacked and said out loud."""
    if not original.startswith(b'LODM') or len(data) < 12:
        return data, False
    want = len(data) - 12
    have = struct.unpack_from('<I', data, 8)[0]
    if want == have:
        return data, False
    return data[:8] + struct.pack('<I', want) + data[12:], True


def first_diff(a, b):
    n = min(len(a), len(b))
    for i in range(n):
        if a[i] != b[i]:
            return i
    return n if len(a) != len(b) else -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ws', default='Commonwealth')
    ap.add_argument('--pair', nargs=2, action='append', metavar=('OLD', 'NEW'), required=True)
    ap.add_argument('--no-rewrite', action='store_true',
                    help='the refuter: compare the raw bytes, which MUST differ')
    ap.add_argument('--quiet', action='store_true')
    a = ap.parse_args()

    subs = spellings(a.ws)
    equal = differ = missing = carried = 0

    for old_p, new_p in a.pair:
        name = os.path.basename(new_p)
        if not os.path.isfile(old_p) or not os.path.isfile(new_p):
            print('  MISSING: %s (old %s, new %s)'
                  % (name, os.path.isfile(old_p), os.path.isfile(new_p)))
            missing += 1
            continue
        o = open(old_p, 'rb').read()
        n = open(new_p, 'rb').read()
        note = ''
        if a.no_rewrite:
            hits = [(s.decode('utf-8', 'replace'), o.count(s)) for s, _ in subs if o.count(s)]
        else:
            o2, hits = rewrite(o, subs)
            o2, packed = repack_lodm(o2, o)
            if packed:
                note = ', LODM payload length repacked'
            o = o2
        if hits:
            carried += 1
        if o == n:
            if not a.quiet:
                print('  identical: %s (%d bytes%s%s)'
                      % (name, len(n), ', %d path string(s) rewritten' % sum(h[1] for h in hits)
                         if hits and not a.no_rewrite else '', note))
            equal += 1
        else:
            i = first_diff(o, n)
            print('  DIFFERS: %s (%d vs %d bytes, first at 0x%X)' % (name, len(o), len(n), i))
            if not a.quiet:
                print('    old: %r' % o[max(0, i - 24):i + 40])
                print('    new: %r' % n[max(0, i - 24):i + 40])
            differ += 1

    print('  %d identical, %d differ, %d missing, %d carried a path string'
          % (equal, differ, missing, carried))

    if a.no_rewrite:
        # the refuter passes when the files that carry a path string DIFFER
        ok = missing == 0 and carried > 0 and differ >= carried
        print('  REFUTER: %s (%d file(s) carry a path string, %d differ without the rewrite)'
              % ('fires' if ok else 'DID NOT FIRE', carried, differ))
        return 0 if ok else 1
    return 0 if (differ == 0 and missing == 0) else 1


if __name__ == '__main__':
    sys.exit(main())
