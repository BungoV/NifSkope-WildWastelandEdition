#!/usr/bin/env python3
"""LANE CELLVIEW4 -- the hook-up, as a REFUSING SCRIPT (skill ww-anchored-hookup).

The lane's own code is in NEW files:

    src/cellsplat.h          the blended ground: types + the vertex-budget count
    src/cellsplat.cpp        the builder
    tests/spells/cell_splat_compare.py   the gate (starts nothing)

This script carries the few lines EXISTING files need so those join the build.
It applies nothing by itself.  `--check` is the default, writes nothing, and
prints a COUNT for every anchor -- never the word "ok", because an anchor that
still matches after the edit was applied says nothing about whether it was
(skill section 4).  Decide "applied or not" from the MARKER strings the
inserted text carries (`lane CELLVIEW4`), never from an anchor.

EVERY ANCHOR IS READ OUT OF THE FILE.  Only a short prefix is typed here; the
script finds the one line that starts with it and uses that line's real bytes,
including its tabs and its trailing comment, and refuses if the line needs
escaping or does not match exactly once (skill section 5a).  Counts were
probed first by scratchpad/cellview4_20260919/anchor_probe.py.

NO ANCHOR IS IN src/lodgen.cpp -- lane IMPOSTORFIX5 owns that file, and this
lane never reads or writes it.

Usage:  python hookup.py [--check | --apply]
"""
import io
import os
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
LF = chr(10)
CR = chr(13)
TAB = chr(9)
MARKER = 'lane CELLVIEW4'


# ---------------------------------------------------------------------------
# the table:  (file, mode, typed prefix, replacement-or-insertion text)
#   mode "after"    inserts TEXT on its own line(s) after the anchor line
#   mode "replace"  substitutes TEXT for the anchor line; TEXT REPEATS the
#                   anchor so a reader can see the edit is additive
# TEXT is written WITHOUT a trailing newline; the script supplies the file's
# own line ending.  Use {A} inside TEXT for the anchor line's real bytes.
# ---------------------------------------------------------------------------
EDITS = [

    # ---- 1. the new files enter the build -------------------------------
    ('NifSkope.pro', 'replace', 'src/cellground.cpp',
     '{A}' + LF + TAB + 'src/cellsplat.cpp \\'),

    ('NifSkope.pro', 'replace', 'src/cellground.h',
     '{A}' + LF + TAB + 'src/cellsplat.h \\'),

    # ---- 2. the ATXT layer index, which is the engine's PAINT ORDER ------
    # src/esmdata.cpp reads the LTEX form and the quadrant byte and stops, so
    # the int16 at ATXT offset 6 -- the order the engine composites the
    # quadrant's layers in -- is thrown away. cellsplat.cpp composites in
    # RECORD order without it and says so in its own legend.
    ('src/esmdata.h', 'replace',
     '//! One additional splat layer on a cell quadrant: 17x17 opacities.',
     '/* THE ATXT LAYER INDEX IS NOW READ (' + MARKER + ').' + LF +
     ' * cellsplat.cpp composites a quadrant\'s layers in this order. Without it' + LF +
     ' * the order is the one the layers happen to sit in the record, which is' + LF +
     ' * not the engine\'s paint order -- so the define is what lets the splat' + LF +
     ' * builder tell a known order from a guess instead of quietly assuming. */' + LF +
     '#define WW_CELLSPLAT_LAYER_INDEX 1' + LF +
     LF +
     '{A}'),

    ('src/esmdata.h', 'replace', 'float opacity[17][17];',
     '{A}' + LF +
     TAB + 'int index = 0;              //!< ATXT layer index: the engine\'s paint order'),

    ('src/esmdata.cpp', 'after', 'layer.ltex = ltex;',
     TAB * 4 + '/* The two fields after the quadrant byte (' + MARKER + '): one' + LF +
     TAB * 4 + ' * unknown byte, then the int16 LAYER INDEX at offset 6. The' + LF +
     TAB * 4 + ' * field is at least 8 bytes (tested above) and 4 + 1 + 1 + 2' + LF +
     TAB * 4 + ' * is exactly 8, so this consumes the payload and no more. */' + LF +
     TAB * 4 + '(void) f.readUInt8();' + LF +
     TAB * 4 + 'layer.index = int( f.readInt16() );'),

    # ---- 3. cellview.cpp can see the new builder ------------------------
    ('src/cellview.cpp', 'replace', '#include "cellground.h"',
     '{A}' + LF +
     '#include "cellsplat.h"\t\t// ' + MARKER),

    # ---- 4. the vertex colour carries a WEIGHT in its alpha byte --------
    # The NIF vertex format needs no change at all: emitBucket already writes
    # a ByteColor4, which already has an alpha byte, and already puts 1.0 in
    # it. Only the float carrying the value upstream was missing.
    ('src/cellview.cpp', 'replace', 'float chan[3] = { 1.0f, 1.0f, 1.0f };',
     TAB + '/* chan[3] is the SPLAT WEIGHT (' + MARKER + '): the layer\'s own VTXT' + LF +
     TAB + ' * opacity at this corner, interpolated across the quad by the' + LF +
     TAB + ' * rasteriser, which IS the engine\'s bilinear blend. 1.0 everywhere' + LF +
     TAB + ' * else, so every existing caller is unchanged. */' + LF +
     TAB + 'float chan[4] = { 1.0f, 1.0f, 1.0f, 1.0f };'),

    ('src/cellview.cpp', 'replace', 'ByteColor4( FloatVector4( o.chan[0]',
     TAB * 5 + 'ByteColor4( FloatVector4( o.chan[0], o.chan[1], o.chan[2], o.chan[3] ) ) );'),

    # ---- 4. an alpha BLEND word for the layer passes --------------------
    # 4844 (0x12EC) is alpha TEST only. A splat layer needs SRC_ALPHA /
    # ONE_MINUS_SRC_ALPHA, which is 4333 (0x10ED). The splat buckets are the
    # only ones that ask for a threshold of 0 -- the mosaic never sets
    # hasAlpha at all, and a model bucket carries lodgen's 128 or a BGSM's own
    # reference -- so the threshold doubles as the discriminator and no new
    # Bucket field is needed.
    ('src/cellview.cpp', 'replace', 'nif->set<int>( iAlpha, "Flags", 4844 );',
     TAB * 3 + '/* 4844 = alpha TEST only; 4333 = alpha BLEND, SRC_ALPHA /' + LF +
     TAB * 3 + ' * ONE_MINUS_SRC_ALPHA, which is what a splat layer needs' + LF +
     TAB * 3 + ' * (' + MARKER + '). A threshold of 0 is asked for by the splat' + LF +
     TAB * 3 + ' * layer buckets and by nothing else. */' + LF +
     TAB * 3 + 'nif->set<int>( iAlpha, "Flags", b.alphaThreshold ? 4844 : 4333 );'),

    # ---- 5. the transparent pass draws in BLOCK order -------------------
    # Scene::drawDeferredShapes calls secondPass.alphaSort(); compareNodesAlpha
    # returns block-number order when BOTH nodes are presorted, and depth order
    # otherwise. `presorted` is set from nif->getBlockIndex(iBlock,
    # "BSOrderedNode"), and Node::drawShapes then marks every child presorted.
    # So the root being a BSOrderedNode is what makes emit order == paint order.
    ('src/cellview.cpp', 'replace', 'QModelIndex iRoot = nif->insertNiBlock(',
     TAB + '/* BSOrderedNode, not NiNode (' + MARKER + '): it marks every child' + LF +
     TAB + ' * presorted, and compareNodesAlpha then sorts the transparent pass' + LF +
     TAB + ' * by BLOCK NUMBER instead of depth -- so the splat layers composite' + LF +
     TAB + ' * in the order cellsplat.cpp emitted their buckets, which is the' + LF +
     TAB + ' * ATXT paint order. It inherits NiNode and is valid for FO4' + LF +
     TAB + ' * (build/nif.xml, BSOrderedNode, FO4 default flags 0x200E).' + LF +
     TAB + ' * THE REFUTER: this changes the sort for EVERY transparent shape in' + LF +
     TAB + ' * the scene, not just the ground. If tree billboards or glass come' + LF +
     TAB + ' * back sorting worse in the downtown picture, the ground needs its' + LF +
     TAB + ' * own BSOrderedNode child instead of the scene root. */' + LF +
     TAB + 'QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "BSOrderedNode" ) );'),

    # ---- 6. the marker filter misses markers at the MESHES ROOT ---------
    # Item 3. Every existing test needs a backslash or the exact suffix
    # `markerx.nif`, so `markerxheading.nif`, `MarkerCOCHeading.nif` and
    # `markers\attachref\...` all pass the filter and get drawn. REFR
    # 0x00066245 (base 0x00000034, XMarkerHeading) is the solid black arrow in
    # scratchpad/cellview3_20260919/images/after_downtown.png -- measured: one
    # reference covers those 497 pure-rgb(0,0,0) pixels and nothing else does.
    ('src/cellview.cpp', 'replace',
     '|| m.endsWith( QLatin1String( "markerx.nif" ) )',
     '{A}' + LF +
     TAB * 2 + '/* AT THE MESHES ROOT THERE IS NO BACKSLASH (' + MARKER + '). Every' + LF +
     TAB * 2 + ' * test above needs one, or the exact suffix `markerx.nif`, so a' + LF +
     TAB * 2 + ' * marker sitting at meshes\\ itself -- `markerxheading.nif`,' + LF +
     TAB * 2 + ' * `markercocheading.nif`, the whole `markers\\...` subtree -- was' + LF +
     TAB * 2 + ' * drawn as an ordinary static. `m` is already lowercased above, so' + LF +
     TAB * 2 + ' * one startsWith covers all of them; a path with a leading folder' + LF +
     TAB * 2 + ' * was already caught by the `\\marker` test. Measured in cell' + LF +
     TAB * 2 + ' * 5,-11: exactly three models take the untextured branch and all' + LF +
     TAB * 2 + ' * three are markers, one of which is the black arrow. */' + LF +
     TAB * 2 + '|| m.startsWith( QLatin1String( "marker" ) )'),

    # ---- 7. the mosaic becomes the FALLBACK -----------------------------
    # THIS EDIT COMES BEFORE THE ONE THAT INSERTS THE SPLAT BLOCK, and the
    # order is load-bearing: the block inserted below contains a line reading
    # `if ( spec.terrain ) {` of its own, so with the other edit first this
    # anchor would match TWICE and the script would refuse -- apply_to
    # re-counts against the text in hand, it does not take the first hit.
    # Narrowing this line first leaves exactly one for the other to find.
    ('src/cellview.cpp', 'replace', 'if ( spec.terrain ) {',
     TAB * 2 + '// the hard-edged mosaic, now the FALLBACK for the blend above (' + MARKER + ')' + LF +
     TAB * 2 + 'if ( spec.terrain && groundBuckets.isEmpty() ) {'),

    # ---- 8. the ground is BLENDED, with the mosaic behind it ------------
    # Anchored on the STATEMENT the block must follow, never on a line ending
    # in `{` (skill section 5), and paired with edit 7 so the mosaic below
    # runs only when this built nothing -- the same shape cellground already
    # uses against the vertex-colour sheet two screens further down.
    ('src/cellview.cpp', 'after', 'QVector<Bucket> groundBuckets;',
     TAB * 2 + '/* THE BLENDED GROUND (' + MARKER + '). The mosaic below takes ONE' + LF +
     TAB * 2 + ' * texture per 128-unit quad, so the ground is a hard-edged grid of' + LF +
     TAB * 2 + ' * tiles. This draws the quadrant\'s BTXT and then every ATXT layer' + LF +
     TAB * 2 + ' * over it in paint order, each with its own VTXT opacity in the' + LF +
     TAB * 2 + ' * vertex colour\'s alpha. It owns the rectangle when it builds' + LF +
     TAB * 2 + ' * anything; the mosaic stays as the fallback, exactly as the' + LF +
     TAB * 2 + ' * mosaic is itself the fallback for the vertex-colour sheet. */' + LF +
     TAB * 2 + 'if ( spec.terrain ) {' + LF +
     TAB * 3 + 'const qint64 splatVerts = cellSplatCountVerts( world, x0, y0, x1, y1 );' + LF +
     TAB * 3 + '/* THE CAP IS CHECKED BEFORE THE ALLOCATION, not after: a splat' + LF +
     TAB * 3 + ' * emits one quad per contributing layer, so a heavily painted' + LF +
     TAB * 3 + ' * block costs a multiple of the mosaic\'s fixed 4096 verts per' + LF +
     TAB * 3 + ' * cell. Refusing here leaves the mosaic to draw the rectangle. */' + LF +
     TAB * 3 + 'if ( splatVerts > 0 && splatVerts <= CELL_MAX_TOTAL_VERTS ) {' + LF +
     TAB * 4 + 'CellSplatBuild sb;' + LF +
     TAB * 4 + 'QString serr;' + LF +
     TAB * 4 + 'if ( cellBuildSplat( world, x0, y0, x1, y1, origin[0], origin[1],' + LF +
     TAB * 5 + 'CELL_GROUND_TILING, sb, &serr ) && !sb.quads.empty() ) {' + LF +
     TAB * 5 + 'landsDrawn = sb.cells;' + LF +
     TAB * 5 + 'groundNote = cellSplatLegend( sb );' + LF +
     TAB * 5 + 'groundBuckets.resize( sb.buckets.size() );' + LF +
     TAB * 5 + 'for ( int bi = 0; bi < sb.buckets.size(); bi++ ) {' + LF +
     TAB * 6 + 'Bucket & gbk = groundBuckets[bi];' + LF +
     TAB * 6 + 'const CellSplatBucket & src = sb.buckets.at( bi );' + LF +
     TAB * 6 + 'gbk.name = src.diffuse.isEmpty()' + LF +
     TAB * 7 + '? QStringLiteral( "landscape" )' + LF +
     TAB * 7 + ': QFileInfo( src.diffuse ).fileName();' + LF +
     TAB * 6 + 'gbk.matString = src.diffuse;' + LF +
     TAB * 6 + 'gbk.normalTex = src.normal;' + LF +
     TAB * 6 + 'gbk.withColour = true;' + LF +
     TAB * 6 + '/* A layer pass blends; a base pass is opaque. Threshold 0 is' + LF +
     TAB * 6 + ' * how emitBucket above tells the two apart. */' + LF +
     TAB * 6 + 'gbk.hasAlpha = src.blend;' + LF +
     TAB * 6 + 'gbk.alphaThreshold = 0;' + LF +
     TAB * 5 + '}' + LF +
     TAB * 5 + 'for ( const CellSplatQuad & q : sb.quads ) {' + LF +
     TAB * 6 + 'if ( q.bucket < 0 || q.bucket >= groundBuckets.size() )' + LF +
     TAB * 7 + 'continue;' + LF +
     TAB * 6 + 'Bucket & gbk = groundBuckets[q.bucket];' + LF +
     TAB * 6 + 'const int base = int( gbk.verts.size() );' + LF +
     TAB * 6 + 'for ( int k = 0; k < 4; k++ ) {' + LF +
     TAB * 7 + 'OutVert o;' + LF +
     TAB * 7 + 'o.pos = Vector3( q.v[k].p[0], q.v[k].p[1], q.v[k].p[2] );' + LF +
     TAB * 7 + 'o.nrm = Vector3( q.nrm[0], q.nrm[1], q.nrm[2] );' + LF +
     TAB * 7 + 'o.tan = Vector3( 1.0f, 0.0f, 0.0f );' + LF +
     TAB * 7 + 'o.bit = Vector3::crossproduct( o.nrm, o.tan );' + LF +
     TAB * 7 + 'if ( o.bit.length() < 1.0e-6f )' + LF +
     TAB * 8 + 'o.bit = Vector3( 0.0f, 1.0f, 0.0f );' + LF +
     TAB * 7 + 'o.uv = Vector2( q.v[k].uv[0], q.v[k].uv[1] );' + LF +
     TAB * 7 + 'for ( int c = 0; c < 3; c++ )' + LF +
     TAB * 8 + 'o.chan[c] = q.v[k].rgb[c];' + LF +
     TAB * 7 + 'o.chan[3] = q.v[k].w;' + LF +
     TAB * 7 + 'gbk.verts.push_back( o );' + LF +
     TAB * 6 + '}' + LF +
     TAB * 6 + 'gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 1 ),' + LF +
     TAB * 7 + 'quint16( base + 2 ) ) );' + LF +
     TAB * 6 + 'gbk.tris.push_back( Triangle( quint16( base ), quint16( base + 2 ),' + LF +
     TAB * 7 + 'quint16( base + 3 ) ) );' + LF +
     TAB * 5 + '}' + LF +
     TAB * 4 + '}' + LF +
     TAB * 3 + '} else if ( splatVerts > CELL_MAX_TOTAL_VERTS ) {' + LF +
     TAB * 4 + 'groundNote = QStringLiteral( "ground: the BLEND refused -- %L1 vertices "' + LF +
     TAB * 5 + '"is past the %L2 cap; the hard-edged mosaic was drawn instead" )' + LF +
     TAB * 5 + '.arg( splatVerts ).arg( qint64( CELL_MAX_TOTAL_VERTS ) );' + LF +
     TAB * 3 + '}' + LF +
     TAB * 2 + '}'),
]


# ---------------------------------------------------------------------------
def resolve(path, prefix):
    """-> (real anchor line without its ending, count, file text, ending).

    The line is READ OUT OF THE FILE, never retyped: only `prefix` is typed,
    and it is matched against the STRIPPED line so the file supplies its own
    tabs, its own trailing comment and its own line ending.
    """
    raw = io.open(ROOT + path, 'rb').read()
    text = raw.decode('utf-8')
    ending = '\r\n' if raw.count(b'\r\n') > raw.count(b'\n') // 2 else LF
    lines = text.split(LF)
    hits = [l for l in lines if l.strip().startswith(prefix)]
    real = hits[0].rstrip(CR) if len(hits) == 1 else None
    return real, len(hits), text, ending


def build():
    """-> (list of (path, old, new, mode, count, real), files touched)."""
    plan = []
    for path, mode, prefix, textTmpl in EDITS:
        real, n, text, ending = resolve(path, prefix)
        plan.append((path, mode, prefix, textTmpl, real, n, ending))
    return plan


def apply_to(text, mode, real, body, ending):
    """Substitute or insert ONCE, on the file's own line ending.

    The count is taken AGAINST THE TEXT IN HAND, not against the file as it
    was when the table was resolved, because an earlier edit in the same run
    can add a line that matches a later anchor.  Taking the first hit would
    silently edit the wrong one; this refuses instead.
    """
    lines = text.split(LF)
    n = sum(1 for ln in lines if ln.rstrip(CR) == real)
    assert n == 1, \
        'the anchor %r matches %d times at apply time, not once -- an earlier ' \
        'edit in this run moved it or duplicated it' % (real[:60], n)
    out = []
    for ln in lines:
        cr = CR if ln.endswith(CR) else ''
        if ln.rstrip(CR) == real:
            if mode != 'replace':
                out.append(ln)
            for b in body.split(LF):
                out.append(b + cr)
        else:
            out.append(ln)
    return LF.join(out)


def main():
    mode = '--check'
    for a in sys.argv[1:]:
        if a in ('--check', '--apply'):
            mode = a
    plan = build()

    ok = True
    byFile = {}
    print('CELLVIEW4 hook-up -- %s' % mode)
    print('%-18s %-8s %-5s %s' % ('file', 'mode', 'count', 'anchor (from the file)'))
    for path, emode, prefix, textTmpl, real, n, ending in plan:
        print('%-18s %-8s %-5d %r' % (path, emode, n,
                                      (real or prefix)[:64]))
        if n != 1:
            ok = False
            print('        REFUSED: the anchor matches %d times, not once' % n)
            continue
        if chr(92) in real or '"' in real:
            # the anchor is used as bytes, not as source: quoting it is not
            # needed, but a line that needs escaping is a line worth seeing
            print('        note: the anchor carries a backslash or a quote; it '
                  'is compared as BYTES read from the file, never retyped')
        byFile.setdefault(path, []).append((emode, real, textTmpl, ending))

    # what is already applied? -- from the MARKER, never from an anchor
    print()
    for path in sorted(byFile):
        raw = io.open(ROOT + path, 'rb').read()
        hits = raw.count(MARKER.encode('utf-8'))
        print('%-18s carries the marker %r %d time(s)' % (path, MARKER, hits))
        if hits:
            print('        this file looks ALREADY APPLIED -- re-applying would '
                  'duplicate the insert; nothing was written')
            ok = False

    if not ok:
        print()
        print('REFUSED: nothing was written.')
        return 1

    # the edits, all or nothing, with the CR count asserted per file
    results = []
    for path in sorted(byFile):
        raw = io.open(ROOT + path, 'rb').read()
        text = raw.decode('utf-8')
        crBefore = text.count(CR)
        added = 0
        ending = LF
        for emode, real, textTmpl, ending in byFile[path]:
            body = textTmpl.replace('{A}', real)
            text = apply_to(text, emode, real, body, ending)
            # lines the edit ADDS: a body of k newlines is k+1 lines, and a
            # "replace" consumes the one line it stands on
            added += body.count(LF) + (0 if emode == 'replace' else 1)
        crAfter = text.count(CR)
        # THE ASSERT. On an LF file the CR count must not move AT ALL: any
        # movement means a CRLF crept in from this script's own text. On a
        # CRLF file it must move by exactly the number of lines added -- one
        # CR per new line and not one more.
        want = crBefore + (added if ending == '\r\n' else 0)
        assert crAfter == want, \
            '%s: CR count %d -> %d, expected %d (%d line(s) added, %s file)' \
            % (path, crBefore, crAfter, want, added,
               'CRLF' if ending == '\r\n' else 'LF')
        results.append((path, text, crBefore, crAfter))
        print('%-18s CR before %d, after %d, %d line(s) added, %d edit(s)'
              % (path, crBefore, crAfter, added, len(byFile[path])))

    if mode == '--check':
        print()
        print('%d of %d anchors match once. NOTHING WAS WRITTEN (--check).'
              % (len(plan), len(plan)))
        return 0

    for path, text, _b, _a in results:
        with io.open(ROOT + path, 'wb') as fh:
            fh.write(text.encode('utf-8'))
    print()
    print('APPLIED %d edits over %d files.' % (len(plan), len(results)))
    print('Now: qmake (NifSkope.pro changed), then the build.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
