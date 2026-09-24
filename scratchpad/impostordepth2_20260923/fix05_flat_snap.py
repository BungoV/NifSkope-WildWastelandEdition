# fix05_flat_snap.py -- lane IMPOSTORDEPTH2, bungo's ruling 2026-09-23 13:1x: the slider's
# CRISP end (0, the default) is FLAT SNAP -- the nearest frame alone, NOT moved by its depth.
# The depth-moved snap stays as a harness override only (WW_IMPOSTOR_SNAP=1 / Options::snap).
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, edits):
    b = open(ROOT + path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new in edits:
        c = s.count(old)
        assert c == 1, (path, old[:70], c)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, path
    open(ROOT + path, 'wb').write(out)
    print('patched', path)


patch('src/gl/impostordraw.h', [
    ('\t *    * 0, the CRISP end and THE DEFAULT, is SNAP: the nearest baked frame\n'
     '\t *      alone at full weight, no blend, placed at its own depth by the\n'
     '\t *      height parallax (`kCrispSearchSteps` of depth search). It jumps\n'
     '\t *      when the nearest frame changes; that is ruled acceptable.\n',
     '\t *    * 0, the CRISP end and THE DEFAULT, is FLAT SNAP (ruled 2026-09-23\n'
     '\t *      13:1x): the nearest baked frame alone at full weight, no blend,\n'
     '\t *      NOT moved by its depth -- the same picture as `heightBlend =\n'
     '\t *      false`. It still writes its depth (`depthOffset`). It jumps when\n'
     '\t *      the nearest frame changes and fails the tear bar; both are ruled\n'
     '\t *      acceptable. Moving a lone frame by its depth thinned the trunk\n'
     '\t *      (T1 203 of 360 views at el 20): it uncovers what that frame never\n'
     '\t *      photographed and no second frame fills it.\n'),
    ('\t//! SNAP (IMPOSTORDEPTH1): forces the slider\'s crisp end whatever `slider`\n'
     '\t//! says -- the nearest frame alone at full weight, keeping the height\n'
     '\t//! parallax and the search -- unlike `heightBlend = false`, which draws\n'
     '\t//! that frame flat on the card plane. `WW_IMPOSTOR_SNAP=1` forces it for\n'
     '\t//! every draw.\n',
     '\t//! THE DEPTH-MOVED SNAP (IMPOSTORDEPTH1), a HARNESS override since the\n'
     '\t//! crisp end became the flat snap (IMPOSTORDEPTH2): the nearest frame\n'
     '\t//! alone at full weight, MOVED by its height parallax and the search --\n'
     '\t//! unlike the crisp end, which draws that frame flat on the card plane.\n'
     '\t//! `WW_IMPOSTOR_SNAP=1` forces it for every draw. Not a user setting: it\n'
     '\t//! fails the trunk bar (tests/spells/impostor_trunk.sh).\n'),
    ('\tbool  snap = true;        //!< the crisp end (or WW_IMPOSTOR_SNAP / Options::snap)\n',
     '\tbool  snap = true;        //!< one frame: the crisp end (or WW_IMPOSTOR_SNAP / Options::snap)\n'
     '\tbool  parallax = false;   //!< the frames are moved by their depth (shader useHeightBlend);\n'
     '\t                          //!< false at the crisp end = the FLAT snap\n'),
])

patch('src/gl/impostordraw.cpp', [
    ('\tr.frameCount = ( opt.heightBlend && !r.snap ) ? 3 : 1;\n',
     '\tr.frameCount = ( opt.heightBlend && !r.snap ) ? 3 : 1;\n'
     '\t/* The crisp end is the FLAT snap (bungo, 2026-09-23 13:1x); only the\n'
     '\t * harness override (WW_IMPOSTOR_SNAP=1 / Options::snap) moves the lone\n'
     '\t * frame by its depth. */\n'
     '\tr.parallax = opt.heightBlend && ( !r.snap || opt.snap || envSnap );\n'),
    ('\tprog->uni1b( "useHeightBlend", opt.heightBlend );\n',
     '\tprog->uni1b( "useHeightBlend", rs.parallax );\n'),
    ('\t/* THE SLIDER (lane IMPOSTORDEPTH2): crisp end = SNAP, the nearest frame\n'
     '\t * alone like the no-blend picture below but keeping the height parallax;\n',
     '\t/* THE SLIDER (lane IMPOSTORDEPTH2): crisp end = FLAT SNAP, the nearest\n'
     '\t * frame alone, the no-blend picture below (WW_IMPOSTOR_SNAP=1 adds the\n'
     '\t * height parallax, a harness override);\n'),
])

patch('src/impostorpreviewtest.cpp', [
    ('\t\t\t.arg( rs.slider <= 0.0f ? QStringLiteral( "the CRISP end, snap" )\n',
     '\t\t\t.arg( rs.slider <= 0.0f ? QStringLiteral( "the CRISP end, flat snap" )\n'),
    ('\t\t\t: rs.snap\n'
     '\t\t\t? QStringLiteral( "frames: ONE, the nearest, with height parallax (%1)" ).arg( rs.snapForced\n'
     '\t\t\t\t? QStringLiteral( "WW_IMPOSTOR_SNAP=1" ) : QStringLiteral( "the slider\'s crisp end" ) )\n',
     '\t\t\t: rs.snap && !rs.parallax\n'
     '\t\t\t? QStringLiteral( "frames: ONE, the nearest, flat on the card plane (the slider\'s crisp end)" )\n'
     '\t\t\t: rs.snap\n'
     '\t\t\t? QStringLiteral( "frames: ONE, the nearest, moved by its depth (%1, a harness override)" ).arg( rs.snapForced\n'
     '\t\t\t\t? QStringLiteral( "WW_IMPOSTOR_SNAP=1" ) : QStringLiteral( "Options::snap" ) )\n'),
])
