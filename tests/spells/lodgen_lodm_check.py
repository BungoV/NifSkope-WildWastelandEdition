#!/usr/bin/env python3
"""Read a `.lodm` card/material sidecar independently -- lane AUDIT1, 2026-09-17.

WHY THIS EXISTS. Step 3 of the final-build audit decodes every output file with
a reader that is not the writer. There was one for every other type in the
family -- `.lodl`, `.lodt`, `.lodo`, `.lodi`, `.lodb`, `.lodj` -- and none for
`.lodm`, so this is the new one, and it is written from `src/io/lodmfile.cpp`
and from the card bake's own documented quantities, not from the JSON the
writer happened to emit.

WHAT IT CHECKS, AND WHERE EACH RULE COMES FROM

  the envelope      `src/io/lodmfile.cpp:8..37`: 'LODM', a little-endian u32
                    version that must be 1, a little-endian u32 payload size
                    that must equal the bytes actually present, and a JSON
                    object whose own `lodm` field is 1 again.
  family / kind     `lodmParse` refuses any family but `legacy` or `pbr`. `kind`
                    defaults to `source`; a card library writes `card`, an array
                    sidecar writes `array`.
  textures          every path recorded must be non-empty, must be spelled with
                    backslashes under `Data\\`, and -- with `--textures-root` --
                    must exist on disk. A sidecar that names a sheet nothing
                    wrote is the failure mode this catches.
  emissiveScale     absent means 1 (`lodmfile.cpp:63`); present it must not be
                    negative. 0 is legal and common: it is what a source that
                    does not own-emit writes.
  the card geometry the quantities the bake computes, recomputed here from the
                    frame alone, so this is a cross-check and not an echo:
                      gap(side)  = max(2, side/16) rounded UP TO EVEN
                      pad(side)  = gap(side) / 2, the margin on each side
                      mips       = log2(min(gapX, gapY)), floored at 1
                    -- bungo's 2026-09-09 ruling, src/lodgen.cpp:2977. The
                    `1 + log2(gap/auxDiv)` shape belongs to the half-resolution
                    AUX sheets and is a different number (lodgen.cpp:3039).
  projection        must be the word `ortho`. A card photographed by the older
                    camera says `persp`, and such a library is the foreshortened
                    vintage that `tools/bake_impostor_cards.sh` says to re-bake.
                    This is the one check that is about a landed RULING rather
                    than about arithmetic, and it is why the word is in the file.
  coverage          floor <= test <= base, all three inside 0..255.
  the library       with `--library`, every card's `oct` must be the library's
                    own, and no card's `base` may exceed its `tile`.

  usage: lodgen_lodm_check.py <file.lodm | dir> [more...]
                              [--library <library.txt>] [--textures-root <dir>]
         lodgen_lodm_check.py --refute <file.lodm>

`--refute` is the floor. It doctors one real sidecar six ways -- the magic, the
version, the declared payload size, the family, the projection word and the gap
-- and requires the SAME checker to refuse each one. A reader that has stopped
reading passes every file in a tree and fails every line of this, which is the
only way to tell the two apart from the outside.

Prints `ok` / `FAIL` lines and exits non-zero on any failure.
"""
import json
import math
import os
import struct
import sys

MAGIC = b'LODM'
VERSION = 1
FAMILIES = ('legacy', 'pbr')
# FIVE kinds are written, not three (lane AUDIT1, 2026-09-17, measured by
# grepping every `root.insert( "kind" )` in src/): card (lodgen.cpp:3071),
# array (:5091), terrainVT (:11804), cardArray (:13479) and aggregate
# (lodgenaggregate.cpp:643), plus `source` as io/lodmfile.cpp:56's default
# when the object carries no kind at all. This reader knew three of them and
# so reported a FAIL against a legitimate aggregate sidecar; the list is the
# reader's bug, not the writer's. An unknown word is still refused -- the
# refuter below proves it -- because a kind nobody writes is how a typo in
# the writer would look.
KINDS = ('source', 'card', 'array', 'terrainVT', 'cardArray', 'aggregate')


class Refused(Exception):
    pass


def parse(raw):
    """the envelope, exactly as src/io/lodmfile.cpp reads it"""
    if len(raw) < 12:
        raise Refused('too short to hold a LODM envelope (%d bytes)' % len(raw))
    if raw[:4] != MAGIC:
        raise Refused('bad magic (expected LODM, got %r)' % raw[:4])
    version = struct.unpack_from('<I', raw, 4)[0]
    if version != VERSION:
        raise Refused('unsupported envelope version %d' % version)
    declared = struct.unpack_from('<I', raw, 8)[0]
    if declared != len(raw) - 12:
        raise Refused('payload size %d does not match the %d bytes present'
                      % (declared, len(raw) - 12))
    try:
        root = json.loads(raw[12:].decode('utf-8'))
    except Exception as e:
        raise Refused('malformed JSON payload: %s' % e)
    if not isinstance(root, dict):
        raise Refused('payload is not a JSON object')
    if root.get('lodm') != VERSION:
        raise Refused('payload is not a lodm 1 object')
    return root


def gap_for(side):
    """bungo's own quantity: max(2, side/16), rounded UP TO EVEN"""
    g = max(2, int(math.ceil(side / 16.0)))
    return g + (g & 1)


def mips_for(mip_unit):
    """how many levels a frame ships: `mips = log2( min( gapX, gapY ) )`

    bungo's ruling of 2026-09-09 evening, quoted in src/lodgen.cpp:2977 -- "SHIP
    ONE MIP FEWER: mips = log2(gap) so the deepest shipped level still has a
    full texel of margin per side" -- and implemented at src/lodgen.cpp:3011 as
    the count of halvings of the gap down to 2, floored at one level.

    NOT `1 + log2(min(gap)/auxDiv)`: that is the count for the HALF-RESOLUTION
    aux sheets (src/lodgen.cpp:3039), which is a different number written to a
    different file. This reader asked for the aux one first and called 23 of 23
    good cards broken, which is the trap worth naming."""
    n = 0
    g = mip_unit
    while g >= 2:
        n += 1
        g //= 2
    return max(1, n)


class Checker(object):
    def __init__(self, library=None, textures_root=None):
        self.fails = 0
        self.checks = 0
        self.library = library or {}
        self.textures_root = textures_root

    def check(self, what, ok):
        self.checks += 1
        if not ok:
            self.fails += 1
        print('  %s %s' % ('ok  ' if ok else 'FAIL', what))
        return ok

    # -- one file ----------------------------------------------------------
    def file(self, path, quiet=False):
        name = os.path.basename(path)
        with open(path, 'rb') as fh:
            raw = fh.read()
        try:
            root = parse(raw)
        except Refused as e:
            self.check('%s: the envelope reads (%s)' % (name, e), False)
            return None
        bad = []
        fam = root.get('family')
        if fam not in FAMILIES:
            bad.append('family %r is not legacy or pbr' % fam)
        kind = root.get('kind', 'source')
        if kind not in KINDS:
            bad.append('kind %r is not one of %s' % (kind, '/'.join(KINDS)))
        # A terrainVT sidecar carries a `terrain` object and NO texture table:
        # the pyramid's sheets live in the .lodt container beside it (measured
        # on the two written by --vt, lane AUDIT1, 2026-09-17). Requiring
        # `textures` of every kind made this reader report two FAILs against
        # legitimate files. The requirement is per kind now, and the terrainVT
        # branch is what stops that from being a hole: the kind still has to
        # carry something, and three of its fields have to be there.
        tex = root.get('textures')
        if kind == 'terrainVT':
            if isinstance(tex, dict) and tex:
                bad.append('a terrainVT sidecar carries a textures object')
            terr = root.get('terrain')
            if not isinstance(terr, dict) or not terr:
                bad.append('kind terrainVT with no terrain object')
            else:
                for k in ('cellUnits', 'content', 'border'):
                    if k not in terr:
                        bad.append('terrain.%s is missing' % k)
            tex = {}
        elif not isinstance(tex, dict) or not tex:
            bad.append('no textures object')
            tex = {}
        for key, val in sorted(tex.items()):
            if not isinstance(val, str) or not val:
                bad.append('texture %s is empty' % key)
                continue
            if '/' in val:
                bad.append('texture %s is spelled with a forward slash' % key)
            if self.textures_root is not None:
                rel = val.replace('\\', '/')
                low = rel.lower()
                if low.startswith('data/'):
                    rel = rel[5:]
                cand = os.path.join(self.textures_root, os.path.basename(rel))
                if not os.path.exists(cand):
                    bad.append('texture %s names %s, which is not on disk'
                               % (key, os.path.basename(rel)))
        es = root.get('emissiveScale', 1)
        if not isinstance(es, (int, float)) or es < 0:
            bad.append('emissiveScale %r is negative or not a number' % es)
        if kind == 'card':
            bad += self.card(root.get('card'))
        if kind == 'array':
            arr = root.get('array')
            if not isinstance(arr, dict):
                bad.append('kind array with no array object')
            else:
                layers = arr.get('layers')
                scales = arr.get('emissiveScale')
                if scales is not None and layers is not None \
                        and len(scales) != len(layers):
                    bad.append('array.emissiveScale has %d entries for %d layers'
                               % (len(scales), len(layers)))
        if not quiet or bad:
            self.check('%s: %s, %s%s'
                       % (name, fam, kind,
                          '' if not bad else ' -- ' + '; '.join(bad[:4])),
                       not bad)
        else:
            self.checks += 1
        return root

    def card(self, c):
        if not isinstance(c, dict):
            return ['kind card with no card object']
        bad = []
        frame = c.get('frame')
        if not (isinstance(frame, list) and len(frame) == 2
                and all(isinstance(v, int) and v > 0 for v in frame)):
            return ['frame %r is not two positive integers' % (frame,)]
        want_gap = [gap_for(frame[0]), gap_for(frame[1])]
        if c.get('gap') != want_gap:
            bad.append('gap %r, but max(2, side/16) rounded up to even is %r'
                       % (c.get('gap'), want_gap))
        want_pad = [want_gap[0] // 2, want_gap[1] // 2]
        if c.get('pad') != want_pad:
            bad.append('pad %r, but half of the gap is %r' % (c.get('pad'), want_pad))
        want_mips = mips_for(min(want_gap))
        if c.get('mips') != want_mips:
            bad.append('mips %r, but log2(min(gap)) is %d'
                       % (c.get('mips'), want_mips))
        if c.get('projection') != 'ortho':
            bad.append('projection %r: this is the foreshortened vintage, re-bake it'
                       % c.get('projection'))
        base = c.get('base')
        if not isinstance(base, int) or base <= 0:
            bad.append('base %r is not a positive integer' % (base,))
        elif frame[0] > base or frame[1] > base:
            bad.append('frame %r does not fit in base %d' % (frame, base))
        oct_ = c.get('oct')
        if not isinstance(oct_, int) or oct_ <= 0 or (oct_ & (oct_ - 1)):
            bad.append('oct %r is not a positive power of two' % (oct_,))
        cov = c.get('coverage')
        if not isinstance(cov, dict):
            bad.append('no coverage object')
        else:
            f, t, b = cov.get('floor'), cov.get('test'), cov.get('base')
            if not all(isinstance(v, int) and 0 <= v <= 255 for v in (f, t, b)):
                bad.append('coverage %r is not three bytes' % (cov,))
            elif not (f <= t <= b):
                bad.append('coverage floor %d <= test %d <= base %d does not hold'
                           % (f, t, b))
        for key in ('depthSpan',):
            v = c.get(key)
            if not isinstance(v, (int, float)) or v <= 0:
                bad.append('%s %r is not positive' % (key, v))
        half = c.get('half')
        if not (isinstance(half, list) and len(half) == 2
                and all(isinstance(v, (int, float)) and v > 0 for v in half)):
            bad.append('half %r is not two positive extents' % (half,))
        if self.library:
            lo = self.library.get('oct')
            if lo is not None and oct_ is not None and str(oct_) != lo:
                bad.append('oct %r but the library says %s' % (oct_, lo))
            lt = self.library.get('tile')
            if lt is not None and isinstance(base, int) and base > int(lt):
                bad.append('base %d exceeds the library tile %s' % (base, lt))
        return bad


def read_library(path):
    out = {}
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            parts = line.split()
            if len(parts) >= 2:
                out[parts[0]] = parts[1]
    return out


def gather(args):
    out = []
    for a in args:
        if os.path.isdir(a):
            for n in sorted(os.listdir(a)):
                if n.lower().endswith('.lodm'):
                    out.append(os.path.join(a, n))
        else:
            out.append(a)
    return out


# -- the floor -------------------------------------------------------------
def refute(path):
    """doctor one real sidecar six ways; every one must be refused"""
    raw = open(path, 'rb').read()
    root = parse(raw)
    ck = Checker()
    print('  the control first, so a checker that refuses everything fails here:')
    ok = ck.file(path)
    if ok is None:
        print('FAIL the control did not read; nothing below proves anything')
        return 1

    def envelope(name, doctored):
        try:
            parse(doctored)
        except Refused as e:
            print('  ok   %s -- refused: %s' % (name, e))
            return 0
        print('  FAIL %s -- ACCEPTED' % name)
        return 1

    def payload(name, mutate):
        d = json.loads(raw[12:].decode('utf-8'))
        mutate(d)
        body = json.dumps(d).encode('utf-8')
        out = MAGIC + struct.pack('<II', VERSION, len(body)) + body
        tmp = path + '.refute.tmp'
        open(tmp, 'wb').write(out)
        try:
            c = Checker()
            # The doctored file's own FAIL line is the thing being PROVOKED, not
            # a failure of this run, so it is held back and the verdict printed
            # in its place.
            keep, sys.stdout = sys.stdout, open(os.devnull, 'w')
            try:
                c.file(tmp, quiet=True)
            finally:
                sys.stdout.close()
                sys.stdout = keep
            if c.fails:
                print('  ok   %s -- refused' % name)
                return 0
            print('  FAIL %s -- ACCEPTED' % name)
            return 1
        finally:
            os.remove(tmp)

    bad = 0
    bad += envelope('the magic, one byte changed', b'LODX' + raw[4:])
    bad += envelope('the envelope version bumped to 2',
                    raw[:4] + struct.pack('<I', 2) + raw[8:])
    bad += envelope('the declared payload size one byte short',
                    raw[:8] + struct.pack('<I', len(raw) - 13) + raw[12:])
    bad += payload('the family set to a word that is neither',
                   lambda d: d.__setitem__('family', 'shiny'))
    if root.get('kind') == 'card':
        bad += payload('the projection word set back to persp',
                       lambda d: d['card'].__setitem__('projection', 'persp'))
        bad += payload('one gap made odd',
                       lambda d: d['card'].__setitem__(
                           'gap', [d['card']['gap'][0] + 1, d['card']['gap'][1]]))
    if root.get('kind') == 'terrainVT':
        # The per-kind textures rule above is only as good as this: a terrainVT
        # sidecar that lost its terrain object, or a field of it, must still be
        # refused, or "no textures required here" would be a hole rather than a
        # rule (lane AUDIT1, 2026-09-17).
        bad += payload('the terrain object removed',
                       lambda d: d.pop('terrain'))
        bad += payload('terrain.cellUnits removed',
                       lambda d: d['terrain'].pop('cellUnits'))
        bad += payload('a textures table added to a terrainVT sidecar',
                       lambda d: d.__setitem__('textures',
                                               {'diffuse': 'textures/x.dds'}))
    print('%d refuter(s) that did NOT catch their mutation' % bad)
    return 1 if bad else 0


def main(argv):
    if '--refute' in argv:
        i = argv.index('--refute')
        return refute(argv[i + 1])
    library, troot, rest = {}, None, []
    i = 0
    while i < len(argv):
        if argv[i] == '--library':
            library = read_library(argv[i + 1]); i += 2
        elif argv[i] == '--textures-root':
            troot = argv[i + 1]; i += 2
        else:
            rest.append(argv[i]); i += 1
    files = gather(rest)
    if not files:
        raise SystemExit(__doc__)
    ck = Checker(library, troot)
    kinds = {}
    for p in files:
        root = ck.file(p, quiet=True)
        if root is not None:
            kinds[root.get('kind', 'source')] = kinds.get(root.get('kind', 'source'), 0) + 1
    # A FLOOR: reading nothing is not a pass.
    ck.check('at least one sidecar was read (%d file(s): %s)'
             % (len(files), ', '.join('%d %s' % (v, k) for k, v in sorted(kinds.items()))),
             len(files) > 0 and sum(kinds.values()) == len(files))
    print('%d checks, %d failures' % (ck.checks, ck.fails))
    return 1 if ck.fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
