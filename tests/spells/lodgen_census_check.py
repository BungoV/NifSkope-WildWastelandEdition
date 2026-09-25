#!/usr/bin/env python3
"""lodgen_census_check.py -- the BAKE half of docs/LODGEN_CENSUS.md.

Given a bake out-dir holding a `.lodo`/`.lodi` pair and the census line the
bake printed, this prints the generator census in the page's format and asserts
that every number the census line promises is DERIVABLE FROM THE FILES -- that
the count the independent decoder reads out of the bytes equals the number the
writer said it wrote.

It is read-only, it needs no exe, and it never consults the writer: the derived
column comes out of `tests/spells/lodgen_native_decode.py`, which parses the
containers from the contract (docs/LODGEN_NATIVE_LODO_LODI.md) rather than from
the emitter.

Three verdict words, and only one of them is a pass:

    ok             the census line and the files agree
    RED            they disagree -- the number printed is not the number written
    not-derivable  the census word is a BAKE-TIME fact (a refusal reason, a
                   corpus read, a before/after measurement) that the container
                   does not carry. Named, counted separately, NEVER counted as
                   a pass, because a checker that quietly skipped them would
                   report a coverage it does not have.

Usage:
    lodgen_census_check.py <out-dir> [--census <log>] [--doctor field=value]...
                           [--self-floor] [--quiet]

    <out-dir>     searched recursively for exactly one `<ws>.lodo` and one
                  `<ws>.lodi`.
    --census      the log the bake wrote. Without it, the out-dir and its two
                  parents are searched for `*.log` files carrying the census
                  lines; if two logs disagree about a line the run REFUSES and
                  names both, because picking the log that agrees would make
                  the whole check circular.
    --doctor      overwrite one parsed CLAIM before comparing -- the floor.
                  `--doctor clusters=20679` must make the run go red.
    --self-floor  run the built-in floor after the green pass: three claims are
                  doctored one at a time and each must be caught by name. A
                  green run with a silent floor is not a gate.

Exit 0 only when every comparison is `ok` and, under --self-floor, every
doctored claim was caught.
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_native_decode as nd            # noqa: E402  (the independent decoder)


# --------------------------------------------------------------- the claims

CENSUS_KINDS = ('native:', 'native-ladder:', 'native-occluders:')


def find_pair(outdir):
    """Exactly one .lodo and one .lodi under outdir, or a refusal by name."""
    lodo, lodi = [], []
    for root, _dirs, files in os.walk(outdir):
        for f in files:
            if f.lower().endswith('.lodo'):
                lodo.append(os.path.join(root, f))
            elif f.lower().endswith('.lodi'):
                lodi.append(os.path.join(root, f))
    if len(lodo) != 1 or len(lodi) != 1:
        raise SystemExit('census REFUSED %s: found %d .lodo and %d .lodi; this checker reads one pair\n  %s'
                         % (outdir, len(lodo), len(lodi), '\n  '.join(sorted(lodo + lodi)) or '(none)'))
    return lodo[0], lodi[0]


def census_lines_from(path):
    out = {}
    with open(path, 'r', encoding='utf-8', errors='replace') as fh:
        for line in fh:
            line = line.rstrip('\n').rstrip('\r')
            for kind in CENSUS_KINDS:
                if line.startswith(kind):
                    out[kind] = line
    return out


def find_census(outdir, given):
    if given:
        lines = census_lines_from(given)
        if not lines:
            raise SystemExit('census REFUSED %s: it carries none of %s'
                             % (given, ', '.join(CENSUS_KINDS)))
        return lines, [given]
    seen, sources = {}, {}
    here = os.path.abspath(outdir)
    roots = [here, os.path.dirname(here), os.path.dirname(os.path.dirname(here))]
    for r in roots:
        if not os.path.isdir(r):
            continue
        for f in sorted(os.listdir(r)):
            if not f.lower().endswith('.log'):
                continue
            p = os.path.join(r, f)
            for kind, line in census_lines_from(p).items():
                if kind in seen and seen[kind] != line:
                    raise SystemExit(
                        'census REFUSED: two logs disagree about the `%s` line.\n'
                        '  %s\n    %s\n  %s\n    %s\n'
                        '  Name the one this bake wrote with --census; choosing the log that agrees '
                        'would make this check circular.'
                        % (kind, sources[kind], seen[kind][:160], p, line[:160]))
                seen.setdefault(kind, line)
                sources.setdefault(kind, p)
    if not seen:
        raise SystemExit('census REFUSED: no *.log under %s or its two parents carries a census line; '
                         'pass --census' % here)
    return seen, sorted(set(sources.values()))


def num(s):
    return float(s) if ('.' in s) else int(s)


def parse_claims(lines):
    """The census lines -> {field: claimed value}. A field the line does not
    carry is simply absent, and the comparison table says `absent`."""
    c = {}
    n = lines.get('native:', '')
    pats = {
        'lodo.fileBytes':        r'\.lodo (\d+) bytes',
        'lodo.baseCount':        r'bases (\d+) of (\d+) in the census',
        'lodo.censusBases':      r'bases \d+ of (\d+) in the census',
        'lodo.basesNoModel':     r'in the census, (\d+) without a loadable model',
        'bake.modelsLoaded':     r'models (\d+) loaded',
        'bake.modelsFailed':     r'models \d+ loaded (\d+) failed',
        'lodo.meshCount':        r'meshes (\d+) clusters',
        'lodo.clusterCount':     r'clusters (\d+) triangles',
        'lodo.triangles':        r'triangles (\d+) vertices',
        'lodo.vertexCount':      r'vertices (\d+) materials',
        'lodo.materialCount':    r'materials (\d+)\)',
        'lodi.fileBytes':        r'\.lodi (\d+) bytes',
        'lodi.instanceCount':    r'instances (\d+) from',
        'bake.arrivals':         r'from (\d+) arrivals',
        'bake.censusRefs':       r'over (\d+) census refs',
        'bake.droppedNoBase':    r'(\d+) dropped for a base outside the table',
        'bake.unlit':            r'(\d+) unlit',
        'bake.noStockIdentity':  r'(\d+) without a stock identity',
        'lodi.presentChunks':    r'chunks (\d+) present of',
        'lodi.chunkCount':       r'present of (\d+) dense',
        'lodi.maxInstPerChunk':  r'max (\d+) a chunk',
        'lodi.maxScale':         r'max scale ([0-9.]+)',
        'lodi.maxBaseId':        r'max baseId (\d+)',
        'pair.bytesPerPlacement': r'([0-9.]+) B a placement',
        'bake.acmrBefore':       r'cacheOrder acmr ([0-9.]+) ->',
        'bake.acmrAfter':        r'cacheOrder acmr [0-9.]+ -> ([0-9.]+)',
        'bake.vertsBefore':      r'verts (\d+) ->',
        'lodo.vertsAfter':       r'verts \d+ -> (\d+)',
        'lodo.silhouetteMeshes': r'silhouette (\d+) meshes',
        'bake.silhouetteOpened': r'silhouette \d+ meshes, (\d+) opened',
    }
    for k, p in pats.items():
        m = re.search(p, n)
        if m:
            c[k] = num(m.group(1))
    c['lodi.partial'] = 1 if 'PARTIAL' in n else 0

    lad = lines.get('native-ladder:', '')
    if lad:
        c['ladder.on'] = 1 if re.match(r'native-ladder: ON', lad) else 0
        for k, p in {
            'lodo.ladderGroup':      r'\(group (\d+),',
            'lodo.levelMax':         r'levels 0\.\.(\d+)',
            'lodo.meshesLaddered':   r'meshes (\d+) with a ladder',
            'lodo.meshesFlat':       r'with a ladder, (\d+) without',
            'lodo.meshesFlatSmall':  r'\((\d+) of those are <= 16 triangles\)',
            'bake.groupsFormed':     r'groups (\d+) formed',
            'bake.refSmall':         r'refused (\d+) too small',
            'bake.refNoCut':         r'(\d+) no triangle removed',
            'bake.refFlat':          r'(\d+) error did not grow',
            'bake.refSilhouette':    r'(\d+) opened a silhouette;',
            'bake.errExact':         r'errors (\d+) measured against full detail',
            'bake.errBounded':       r'full detail, (\d+) chain-bounded',
            'bake.weldedVerts':      r'welded verts (\d+)',
            'bake.uvConflicts':      r'uv conflicts (\d+)',
            'lodo.maxError':         r'maxError ([0-9.]+)',
            'lodo.coneOpen':         r'coneOpen (\d+) clusters',
            'lodo.roots':            r'roots (\d+) covering',
            'lodo.rootTriangles':    r'covering (\d+) full-detail triangles',
            'bake.coarsestOpened':   r'silhouette on (\d+) meshes',
        }.items():
            m = re.search(p, lad)
            if m:
                c[k] = num(m.group(1))
        for m in re.finditer(r'level (\d+) clusters (\d+) triangles (\d+) meanError ([0-9.]+)', lad):
            lv = int(m.group(1))
            c['lodo.level%d.clusters' % lv] = int(m.group(2))
            c['lodo.level%d.triangles' % lv] = int(m.group(3))
            c['lodo.level%d.meanError' % lv] = float(m.group(4))
        # THE LADDER OFF (the default since the authored-only library): the line
        # still prints `(group 4, ...)`, the target a ladder WOULD be built at,
        # while the file writes ladderGroup 0 -- "0 iff the LADDER flag is clear"
        # (NATIVE sec 3, header 0xCD). So the claim about the FILE is 0, and the
        # printed target is a setting of a pass that did not run. Lane INCRGATE1,
        # 2026-09-24: the first v4 run of this checker (a default Sanctuary pair)
        # read `lodo.ladderGroup 4 / 0 RED` on a correct file.
        if c.get('ladder.on') == 0 and 'lodo.ladderGroup' in c:
            c['bake.ladderGroupTarget'] = c['lodo.ladderGroup']
            c['lodo.ladderGroup'] = 0

    occ = lines.get('native-occluders:', '')
    if occ:
        c['occluders.on'] = 1 if re.match(r'native-occluders: ON', occ) else 0
        for k, p in {
            'lodi.occluderCount':      r'boxes (\d+) written',
            'bake.boxesOffered':       r'written from (\d+) offered',
            'bake.boxesDropped':       r'offered, (\d+) dropped',
            'lodi.maxOccludersPerCell': r'\(cap (\d+) a cell\)',
            'lodi.cellsWithBox':       r'cells (\d+) of \d+ populated',
            'lodi.cellsPopulated':     r'cells \d+ of (\d+) populated',
            'lodi.cellsWithBoxPct':    r'have one \(([0-9.]+) percent\)',
            'lodi.cellsWithoutBox':    r'percent\), (\d+) with none',
            'bake.modelsFitted':       r'models fitted (\d+)',
            'bake.refNotWatertight':   r'refused (\d+) not watertight',
            'bake.refTooSmall':        r'(\d+) too small \(< 256 u diagonal\)',
            'bake.refNoInterior':      r'(\d+) no interior voxel',
            'bake.refTooThin':         r'(\d+) too thin',
            'bake.refProbe':           r'(\d+) failed the 100-point probe',
        }.items():
            m = re.search(p, occ)
            if m:
                c[k] = num(m.group(1))
    return c


# ------------------------------------------------------------- the derivation

def derive(lodo_path, lodi_path):
    """Every number this checker can read straight out of the two files."""
    L = nd.read_lodo(lodo_path)
    T = nd.read_lodi(lodi_path)
    h, ih = L['header'], T['header']
    d = {}

    d['lodo.fileBytes'] = os.path.getsize(lodo_path)
    d['lodi.fileBytes'] = os.path.getsize(lodi_path)
    d['lodo.baseCount'] = h['baseCount']
    d['lodo.meshCount'] = h['meshCount']
    d['lodo.clusterCount'] = h['clusterCount']
    d['lodo.vertexCount'] = h['vertexCount']
    d['lodo.vertsAfter'] = h['vertexCount']
    d['lodo.materialCount'] = h['materialCount']
    d['lodo.triangles'] = sum(c['triangleCount'] for c in L['clusters'])
    d['lodo.silhouetteMeshes'] = h['meshCount']
    d['lodo.levelMax'] = h['levelMax']
    d['lodo.ladderGroup'] = h['ladderGroup']
    d['ladder.on'] = 1 if (h['flags'] & 8) else 0

    lods = L['clusterLods']
    d['lodo.coneOpen'] = sum(1 for c in L['clusters'] if c['flags'] & 4)
    roots = [i for i, cl in enumerate(lods) if cl['parentFirst'] == 0xFFFFFFFF]
    d['lodo.roots'] = len(roots)
    d['lodo.rootTriangles'] = sum(lods[i]['sourceTriangles'] for i in roots)
    d['lodo.maxError'] = round(max((cl['geometricError'] for cl in lods), default=0.0), 4)

    per = {}
    for i, cl in enumerate(lods):
        lv = cl['level']
        s = per.setdefault(lv, [0, 0, 0.0])
        s[0] += 1
        s[1] += L['clusters'][i]['triangleCount']
        s[2] += cl['geometricError']
    for lv, (n, t, e) in per.items():
        d['lodo.level%d.clusters' % lv] = n
        d['lodo.level%d.triangles' % lv] = t
        d['lodo.level%d.meanError' % lv] = round(e / n, 4)

    laddered = 0
    flat_small = 0
    for m in L['meshes']:
        first, cnt = m['clusterFirst'], m['clusterCount']
        levels = {lods[i]['level'] for i in range(first, first + cnt)}
        tris_l0 = sum(L['clusters'][i]['triangleCount']
                      for i in range(first, first + cnt) if lods[i]['level'] == 0)
        if max(levels) > 0:
            laddered += 1
        elif tris_l0 <= 16:
            flat_small += 1
    d['lodo.meshesLaddered'] = laddered
    d['lodo.meshesFlat'] = h['meshCount'] - laddered
    d['lodo.meshesFlatSmall'] = flat_small

    d['lodi.instanceCount'] = ih['instanceCount']
    d['lodi.presentChunks'] = ih['presentChunks']
    d['lodi.chunkCount'] = ih['chunkCount']
    d['lodi.maxInstPerChunk'] = ih['maxInstancesPerChunk']
    d['lodi.partial'] = 1 if (ih['flags'] & 2) else 0
    d['lodi.maxScale'] = round(max(r['scaleF'] for r in T['instances']), 4)   # v10: 8 + u16/8192 under bit 7
    d['lodi.maxBaseId'] = max(r['baseId'] for r in T['instances'])
    d['lodi.occluderCount'] = ih['occluderCount']
    d['lodi.maxOccludersPerCell'] = ih['maxOccludersPerCell']

    populated = with_box = 0
    for k, cr in enumerate(T['cellRanges']):
        if cr[1]:                                   # (instanceFirst, instanceCount)
            populated += 1
            if T['occluderRanges'][k][1]:           # (occluderFirst, occluderCount)
                with_box += 1
    d['lodi.cellsPopulated'] = populated
    d['lodi.cellsWithBox'] = with_box
    d['lodi.cellsWithoutBox'] = populated - with_box
    d['lodi.cellsWithBoxPct'] = round(100.0 * with_box / populated, 1) if populated else 0.0
    d['occluders.on'] = 1 if ih['occluderCount'] or ih['maxOccludersPerCell'] else 0

    d['pair.bytesPerPlacement'] = round(
        (d['lodo.fileBytes'] + d['lodi.fileBytes']) / float(ih['instanceCount']), 1) \
        if ih['instanceCount'] else 0.0
    return d, L, T


# The census words the containers cannot carry. Each is a bake-time fact -- a
# refusal reason, a corpus read, a before/after measurement -- and each is
# printed by name rather than skipped.
NOT_DERIVABLE = {
    'bake.ladderGroupTarget': 'the grouping target of a ladder that was not built (the file carries 0)',
    'lodo.censusBases':      'the ESM census count; the file holds only the bases it wrote',
    'lodo.basesNoModel':     'a bake-time load failure',
    'bake.modelsLoaded':     'a bake-time corpus read',
    'bake.modelsFailed':     'a bake-time corpus read',
    'bake.arrivals':         'placements offered to the writer',
    'bake.censusRefs':       'references the ESM walk read',
    'bake.droppedNoBase':    'a bake-time drop',
    'bake.unlit':            'a bake-time shading state',
    'bake.noStockIdentity':  'a bake-time join against the stock manifests',
    'bake.acmrBefore':       'the order BEFORE the cache pass; only the after order is in the file',
    'bake.acmrAfter':        'measured over the source topology, not stored',
    'bake.vertsBefore':      'the pre-permutation vertex count',
    'bake.silhouetteOpened': 'a boundary-edge comparison against the SOURCE mesh',
    'bake.groupsFormed':     'a ladder-build outcome',
    'bake.refSmall':         'a ladder refusal reason',
    'bake.refNoCut':         'a ladder refusal reason',
    'bake.refFlat':          'a ladder refusal reason',
    'bake.refSilhouette':    'a ladder refusal reason',
    'bake.errExact':         'which error rule served',
    'bake.errBounded':       'which error rule served',
    'bake.weldedVerts':      'a simplifier intermediate',
    'bake.uvConflicts':      'a simplifier intermediate',
    'bake.coarsestOpened':   'a boundary-edge comparison against level 0',
    'bake.boxesOffered':     'occluder candidates before the per-cell cap',
    'bake.boxesDropped':     'occluder candidates the cap dropped',
    'bake.modelsFitted':     'a per-MODEL fit outcome; the file holds per-CELL boxes',
    'bake.refNotWatertight': 'an occluder refusal reason',
    'bake.refTooSmall':      'an occluder refusal reason',
    'bake.refNoInterior':    'an occluder refusal reason',
    'bake.refTooThin':       'an occluder refusal reason',
    'bake.refProbe':         'an occluder refusal reason',
}

# Where the page says each derived field is read from. Gate C3 checks that each
# of these section numbers exists in the contract page.
SOURCE = {
    'lodo.fileBytes': 'NATIVE sec 3 header 0xB0', 'lodi.fileBytes': 'NATIVE sec 4 header 0x88',
    'lodo.baseCount': 'NATIVE sec 3 header 0x50', 'lodo.meshCount': 'NATIVE sec 3 header 0x54',
    'lodo.clusterCount': 'NATIVE sec 3 header 0x58', 'lodo.materialCount': 'NATIVE sec 3 header 0x5C',
    'lodo.vertexCount': 'NATIVE sec 3 header 0x60', 'lodo.vertsAfter': 'NATIVE sec 3 header 0x60',
    'lodo.triangles': 'NATIVE sec 3.2 cluster table', 'lodo.silhouetteMeshes': 'NATIVE sec 3 header 0x54',
    'lodo.levelMax': 'NATIVE sec 3 header 0xCC', 'lodo.ladderGroup': 'NATIVE sec 3 header 0xCD',
    'ladder.on': 'NATIVE sec 3 header 0x08 bit3', 'lodo.coneOpen': 'NATIVE sec 3.6 cluster flags bit 2',
    'lodo.roots': 'NATIVE sec 3.2 ladder parentFirst', 'lodo.rootTriangles': 'NATIVE sec 3.2 ladder sourceTriangles',
    'lodo.maxError': 'NATIVE sec 3.2 ladder geometricError', 'lodo.meshesLaddered': 'NATIVE sec 3.2 mesh levelCount',
    'lodo.meshesFlat': 'NATIVE sec 3.2 mesh levelCount', 'lodo.meshesFlatSmall': 'NATIVE sec 3.2 mesh table',
    'lodi.instanceCount': 'NATIVE sec 4 header 0x58', 'lodi.presentChunks': 'NATIVE sec 4 header 0x5C',
    'lodi.chunkCount': 'NATIVE sec 4 header 0x54', 'lodi.maxInstPerChunk': 'NATIVE sec 4 header 0x60',
    'lodi.partial': 'NATIVE sec 4 header 0x08 bit1', 'lodi.maxScale': 'NATIVE sec 4.1 instance scale',
    'lodi.maxBaseId': 'NATIVE sec 4.1 instance baseId', 'lodi.occluderCount': 'NATIVE sec 4 header 0xA8',
    'lodi.maxOccludersPerCell': 'NATIVE sec 4 header 0xAE', 'lodi.cellsPopulated': 'NATIVE sec 4 cell-range blob',
    'lodi.cellsWithBox': 'NATIVE sec 4.5.1 occluder cell ranges',
    'lodi.cellsWithoutBox': 'NATIVE sec 4.5.1 occluder cell ranges',
    'lodi.cellsWithBoxPct': 'NATIVE sec 4.5.1 occluder cell ranges',
    'occluders.on': 'NATIVE sec 4 header 0xAE', 'pair.bytesPerPlacement': 'NATIVE sec 1',
}
for _lv in range(16):
    SOURCE['lodo.level%d.clusters' % _lv] = 'NATIVE sec 3.5.3 ladder level'
    SOURCE['lodo.level%d.triangles' % _lv] = 'NATIVE sec 3.5.3 ladder level'
    SOURCE['lodo.level%d.meanError' % _lv] = 'NATIVE sec 3.5.3 ladder level'


def close(a, b):
    if isinstance(a, float) or isinstance(b, float):
        return abs(float(a) - float(b)) <= 0.05 + 1e-9
    return a == b


def run(outdir, census, doctor, quiet):
    lodo_path, lodi_path = find_pair(outdir)
    lines, sources = find_census(outdir, census)
    claims = parse_claims(lines)
    for one in doctor:
        k, _, v = one.partition('=')
        if k not in claims:
            raise SystemExit('census REFUSED --doctor %s: the census line carries no `%s`' % (one, k))
        claims[k] = num(v)
    try:
        derived, L, _T = derive(lodo_path, lodi_path)
    except nd.Refusal as e:
        # The decoder refused the pair. That is not a census disagreement and it
        # is not reported as one: the files could not be read at all, so no
        # number below is derivable. Exit code 2, and the reason in words.
        print('census REFUSED %s / %s: the independent decoder will not read this pair --\n  %s'
              % (lodo_path, lodi_path, e))
        raise SystemExit(2)

    rows, ok, red, nd_rows = [], 0, 0, 0
    for k in sorted(set(list(claims) + list(derived))):
        cl = claims.get(k, None)
        dv = derived.get(k, None)
        if k in NOT_DERIVABLE:
            rows.append((k, cl, '--', 'not-derivable', NOT_DERIVABLE[k]))
            nd_rows += 1
            continue
        if cl is None:
            rows.append((k, 'absent', dv, 'not-claimed', 'the census line does not carry this word'))
            continue
        if dv is None:
            rows.append((k, cl, 'absent', 'RED', 'claimed but this checker derives nothing for it'))
            red += 1
            continue
        good = close(cl, dv)
        rows.append((k, cl, dv, 'ok' if good else 'RED', SOURCE.get(k, '')))
        ok += good
        red += (not good)

    if not quiet:
        print('BAKE CENSUS  %s  %s' % (L['header']['worldspace'], os.path.abspath(outdir)))
        print('  pair    %s' % lodo_path)
        print('          %s' % lodi_path)
        for s in sources:
            print('  census  %s' % s)
        print('')
        print('  %-26s %14s %14s  %-13s %s' % ('field', 'census says', 'files say', 'verdict', 'read from'))
        for k, cl, dv, verdict, note in rows:
            print('  %-26s %14s %14s  %-13s %s' % (k, cl, dv, verdict, note))
        print('')
    print('%d checks, %d failures, %d census words the files cannot carry'
          % (ok + red, red, nd_rows))
    return red


def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument('outdir')
    ap.add_argument('--census')
    ap.add_argument('--doctor', action='append', default=[])
    ap.add_argument('--self-floor', action='store_true')
    ap.add_argument('--quiet', action='store_true')
    a = ap.parse_args()

    red = run(a.outdir, a.census, a.doctor, a.quiet)
    if red and not a.doctor:
        print('CENSUS RED: %d of the census line numbers are not what the files hold' % red)
        return 1
    if a.doctor:
        if red:
            print('FLOOR ok: the doctored claim(s) %s were caught' % ', '.join(a.doctor))
            return 0
        print('FLOOR FAILED: the doctored claim(s) %s passed -- this checker cannot fail on its input'
              % ', '.join(a.doctor))
        return 1

    if a.self_floor:
        print('')
        print('-- the floor: three claims doctored one at a time, each must be caught --')
        bad = 0
        for one in ('lodo.clusterCount=20679', 'lodi.instanceCount=3527', 'lodo.level1.triangles=68449'):
            k = one.split('=')[0]
            try:
                r = run(a.outdir, a.census, [one], True)
            except SystemExit as e:
                print('  skip %-28s %s' % (k, e))
                continue
            print('  %-28s %s' % (one, 'caught' if r else 'NOT CAUGHT'))
            bad += (r == 0)
        if bad:
            print('FLOOR FAILED: %d doctored claim(s) passed' % bad)
            return 1
        print('FLOOR ok')
    return 0


if __name__ == '__main__':
    sys.exit(main())
