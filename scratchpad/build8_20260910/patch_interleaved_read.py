#!/usr/bin/env python3
"""Lane BUILD8: teach src/hkxanim.cpp to READ what src/hkxwrite.cpp writes.

THE MEASURED BLOCKER, 2026-09-10 14:3x.  The round trip the brief orders ends
in an `hkaInterleavedUncompressedAnimation` (that is the only class lane HKX5's
writer emits, and lane HKXCLASS measured that it is the one FO4 implements
fully).  Lane HKX1's reader dispatches on the class NAME and refuses everything
that is not `hkaSplineCompressedAnimation`:

    hkx-tsv: animation 0 is a hkaInterleavedUncompressedAnimation,
             not decoded by this reader

So NifSkope cannot open the .hkx its own glTF import just wrote, and the
PICTURE half of the brief -- five frames rendered from the round-tripped clip
through `WW_HKXANIM_CLIP`, which goes through this same reader -- is
impossible without this arm.

CROSS-LANE, AND DELIBERATE.  `src/hkxanim.cpp` is lane HKX2b's file
(CONSTITUTION 1, one lane per file).  No HKX lane is alive in this tree; the
change is ADDITIVE -- every class other than the two is refused with the same
sentence as before, the spline path is byte-for-byte the code it was, and
lane HKX1's and HKX2's gates re-run on it unchanged.  It is named in the lane
report, in MISTAKES.md and in the handoff.

The layout is not guessed.  docs/HKX_WRITE_FORMAT.md section 3 and 3.1 give it
from the engine's own reflection and from the disassembly of
`hkaInterleavedUncompressedAnimation::transformTrack` (rva 0x01fa1ac0):
`hkArray<hkQsTransform>` at +0x38, 48 bytes per element, FRAME-MAJOR, and the
frame count is `transforms.size / numberOfTransformTracks` because the object
states it nowhere else.

Four edits, each anchored exactly once.  --check is the default.
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
F = "src/hkxanim.cpp"
MARKER = b"hkaInterleavedUncompressedAnimation"

# ---------------------------------------------------------------- 1. RawAnim
RAW_ANCHOR = "\tQVector<QVector<HkxAnnotation>> annotations;\n\tbool hasMotion = false;\n"
RAW_TEXT = (
    "\tQVector<QVector<HkxAnnotation>> annotations;\n"
    "\t//! hkaInterleavedUncompressedAnimation ONLY (lane BUILD8): the whole\n"
    "\t//! hkArray<hkQsTransform> as 12 floats per element, frame-major --\n"
    "\t//! 12 * numTransformTracks * numFrames entries.  Empty for a spline clip,\n"
    "\t//! and that emptiness is what selects the decoder arm.\n"
    "\tQVector<float> interleaved;\n"
    "\tbool hasMotion = false;\n"
)

# ---------------------------------------------------------------- 2. validate
VAL_ANCHOR = (
    "\tfor ( const RawAnim & a : r.anims ) {\n"
    "\t\tif ( a.type != 3 )\n"
)
VAL_TEXT = (
    "\tfor ( const RawAnim & a : r.anims ) {\n"
    "\t\t/* THE INTERLEAVED ARM (lane BUILD8).  An\n"
    "\t\t * hkaInterleavedUncompressedAnimation has no blocks, no quantization\n"
    "\t\t * mask and no stored frame count, so none of the spline rules below\n"
    "\t\t * apply to it and every one of them would refuse a well-formed file.\n"
    "\t\t * What CAN be checked is checked here. */\n"
    "\t\tif ( !a.interleaved.isEmpty() ) {\n"
    "\t\t\tif ( a.type != 1 )\n"
    "\t\t\t\trefuse( QString( \"hkaAnimation::type is %1; HK_INTERLEAVED_ANIMATION is 1\" ).arg( a.type ) );\n"
    "\t\t\tif ( a.numTransformTracks < 1 || a.numFrames < 1 )\n"
    "\t\t\t\trefuse( QString( \"numTransformTracks %1 / numFrames %2\" ).arg( a.numTransformTracks ).arg( a.numFrames ) );\n"
    "\t\t\tif ( a.interleaved.size() != 12 * a.numTransformTracks * a.numFrames )\n"
    "\t\t\t\trefuse( QString( \"transforms holds %1 floats, not 12 * %2 tracks * %3 frames\" )\n"
    "\t\t\t\t\t\t.arg( a.interleaved.size() ).arg( a.numTransformTracks ).arg( a.numFrames ) );\n"
    "\t\t\tif ( std::fabs( a.duration - ( a.numFrames - 1 ) * a.frameDuration ) > 1e-3 * std::max( 1.0f, a.duration ) )\n"
    "\t\t\t\trefuse( QString( \"duration %1 is not (numFrames-1) * frameDuration = %2\" )\n"
    "\t\t\t\t\t\t.arg( a.duration ).arg( ( a.numFrames - 1 ) * a.frameDuration ) );\n"
    "\t\t\tif ( a.hasMotion && a.motion.samples.size() != 4 * a.numFrames )\n"
    "\t\t\t\trefuse( QString( \"%1 root-motion samples for %2 frames\" ).arg( a.motion.samples.size() / 4 ).arg( a.numFrames ) );\n"
    "\t\t\tcontinue;\n"
    "\t\t}\n"
    "\t\tif ( a.type != 3 )\n"
)

# ---------------------------------------------------------------- 3. decodeClip
DEC_ANCHOR = "\t// parse every block once\n"
DEC_TEXT = (
    "\t/* THE INTERLEAVED ARM (lane BUILD8): the frames ARE the file.  Nothing\n"
    "\t * is evaluated, so worstQuatLengthDeviation and the block-overlap\n"
    "\t * diagnostics stay at their zero defaults -- there are no blocks and no\n"
    "\t * spline to be off unit. */\n"
    "\tif ( !a.interleaved.isEmpty() ) {\n"
    "\t\tclip.frames.resize( a.numFrames );\n"
    "\t\tfor ( int f = 0; f < a.numFrames; f++ ) {\n"
    "\t\t\tQVector<HkxTransform> & row = clip.frames[f];\n"
    "\t\t\trow.resize( a.numTransformTracks );\n"
    "\t\t\tfor ( int t = 0; t < a.numTransformTracks; t++ ) {\n"
    "\t\t\t\tconst float * v = a.interleaved.constData() + 12 * ( f * a.numTransformTracks + t );\n"
    "\t\t\t\tHkxTransform & x = row[t];\n"
    "\t\t\t\tx.translation = Vector3( v[0], v[1], v[2] );\t\t// v[3] is hkVector4's pad\n"
    "\t\t\t\tx.rotation = Quat( v[7], v[4], v[5], v[6] );\t\t// Havok (x,y,z,w) -> Quat (w,x,y,z)\n"
    "\t\t\t\tx.scale = Vector3( v[8], v[9], v[10] );\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\treturn clip;\n"
    "\t}\n"
    "\n"
    "\t// parse every block once\n"
)

# ---------------------------------------------------------------- 4. packfile
PF_ANCHOR = '''			const QString cls = pf.classOf.value( ao );
			if ( cls != QLatin1String( "hkaSplineCompressedAnimation" ) )
				refuse( QString( "animation %1 is a %2, not decoded by this reader" ).arg( i ).arg( cls ) );
			RawAnim a;
			a.ref = QString( "#%1" ).arg( ao - pf.base );
			a.type = pf.b.i32( ao + 0x10 );
			a.duration = pf.b.f32( ao + 0x14 );
			a.numTransformTracks = pf.b.i32( ao + 0x18 );
			a.numFloatTracks = pf.b.i32( ao + 0x1c );
			a.numFrames = pf.b.i32( ao + 0x38 );
			a.numBlocks = pf.b.i32( ao + 0x3c );
			a.maxFramesPerBlock = pf.b.i32( ao + 0x40 );
			a.maskAndQuantizationSize = pf.b.i32( ao + 0x44 );
			a.blockDuration = pf.b.f32( ao + 0x48 );
			a.blockInverseDuration = pf.b.f32( ao + 0x4c );
			a.frameDuration = pf.b.f32( ao + 0x50 );
			a.blockOffsets = u32Array( pf, ao + 0x58 );
			a.floatBlockOffsets = u32Array( pf, ao + 0x68 );
			a.transformOffsets = u32Array( pf, ao + 0x78 );
			a.floatOffsets = u32Array( pf, ao + 0x88 );
			int dn = 0;
			const qint64 dp = pf.arr( ao + 0x98, 1, dn );
			if ( dn )
				a.data = QByteArray( reinterpret_cast<const char *>( pf.b.d + dp ), dn );
			a.endian = pf.b.i32( ao + 0xa8 );
'''
PF_TEXT = '''			const QString cls = pf.classOf.value( ao );
			// lane BUILD8: the reader now serves BOTH classes FO4 uses. Every
			// other class is refused with exactly the sentence it was before.
			const bool isInterleaved = ( cls == QLatin1String( "hkaInterleavedUncompressedAnimation" ) );
			if ( cls != QLatin1String( "hkaSplineCompressedAnimation" ) && !isInterleaved )
				refuse( QString( "animation %1 is a %2, not decoded by this reader" ).arg( i ).arg( cls ) );
			RawAnim a;
			a.ref = QString( "#%1" ).arg( ao - pf.base );
			// hkaAnimation's OWN members, shared by both subclasses: type +0x10,
			// duration +0x14, numberOfTransformTracks +0x18, numberOfFloatTracks
			// +0x1c, extractedMotion +0x20, annotationTracks +0x28. Only +0x38
			// onward is subclass territory.
			a.type = pf.b.i32( ao + 0x10 );
			a.duration = pf.b.f32( ao + 0x14 );
			a.numTransformTracks = pf.b.i32( ao + 0x18 );
			a.numFloatTracks = pf.b.i32( ao + 0x1c );
			if ( isInterleaved ) {
				/* +0x38 hkArray<hkQsTransform>, 48 bytes each, FRAME-MAJOR:
				 * element = data + 48 * (frame * numberOfTransformTracks + track).
				 * The object states its frame count NOWHERE -- the engine
				 * divides transforms.size by numberOfTransformTracks
				 * (hkaInterleavedUncompressedAnimation::transformTrack, rva
				 * 0x01fa1ac0; docs/HKX_WRITE_FORMAT.md section 3.1), and so
				 * does this. */
				if ( a.numTransformTracks < 1 )
					refuse( QString( "animation %1 has %2 transform tracks" ).arg( i ).arg( a.numTransformTracks ) );
				int xn = 0;
				const qint64 xp = pf.arr( ao + 0x38, 48, xn );
				if ( xn % a.numTransformTracks )
					refuse( QString( "transforms has %1 elements, not a whole multiple of the %2 transform tracks" )
							.arg( xn ).arg( a.numTransformTracks ) );
				a.numFrames = xn / a.numTransformTracks;
				a.numBlocks = 0;			// an interleaved clip has no blocks
				a.maxFramesPerBlock = 0;
				a.frameDuration = a.numFrames > 1 ? a.duration / float( a.numFrames - 1 ) : 0.0f;
				a.interleaved.resize( 12 * xn );
				for ( int k = 0; k < 12 * xn; k++ )
					a.interleaved[k] = pf.b.f32( xp + 4 * k );
			} else {
			a.numFrames = pf.b.i32( ao + 0x38 );
			a.numBlocks = pf.b.i32( ao + 0x3c );
			a.maxFramesPerBlock = pf.b.i32( ao + 0x40 );
			a.maskAndQuantizationSize = pf.b.i32( ao + 0x44 );
			a.blockDuration = pf.b.f32( ao + 0x48 );
			a.blockInverseDuration = pf.b.f32( ao + 0x4c );
			a.frameDuration = pf.b.f32( ao + 0x50 );
			a.blockOffsets = u32Array( pf, ao + 0x58 );
			a.floatBlockOffsets = u32Array( pf, ao + 0x68 );
			a.transformOffsets = u32Array( pf, ao + 0x78 );
			a.floatOffsets = u32Array( pf, ao + 0x88 );
			int dn = 0;
			const qint64 dp = pf.arr( ao + 0x98, 1, dn );
			if ( dn )
				a.data = QByteArray( reinterpret_cast<const char *>( pf.b.d + dp ), dn );
			a.endian = pf.b.i32( ao + 0xa8 );
			}
'''

EDITS = [(RAW_ANCHOR, RAW_TEXT), (VAL_ANCHOR, VAL_TEXT), (DEC_ANCHOR, DEC_TEXT), (PF_ANCHOR, PF_TEXT)]


def main(argv):
    apply_it = "--apply" in argv
    with open(os.path.join(REPO, F), "rb") as fh:
        raw = fh.read()
    before = (len(raw), raw.count(b"\r"), raw.count(b"\n"))
    ok = True
    for i, (anchor, _t) in enumerate(EDITS):
        n = raw.count(anchor.encode("utf-8"))
        print("edit %d  anchor x%d  %s" % (i + 1, n, repr(anchor[:56])))
        if n != 1:
            ok = False
    already = raw.count(MARKER)
    print("\nanchors ok: %s; %r already present: %d occurrence(s)" % (ok, MARKER.decode(), already))
    print("  %s %d bytes, CR %d, LF %d" % (F, before[0], before[1], before[2]))
    if not apply_it:
        print("\n--check only: nothing written.")
        return 0 if ok else 1
    if not ok:
        print("\nREFUSED: an anchor does not match exactly once.")
        return 1
    if already:
        print("\nREFUSED: the class name is already in the file.")
        return 1
    for anchor, text in EDITS:
        a, t = anchor.encode("utf-8"), text.encode("utf-8")
        assert raw.count(a) == 1
        raw = raw.replace(a, t)
    with open(os.path.join(REPO, F), "wb") as fh:
        fh.write(raw)
    nb, cr, lf = len(raw), raw.count(b"\r"), raw.count(b"\n")
    print("wrote %s %d bytes (%+d), CR %d (was %d), LF %d (was %d)"
          % (F, nb, nb - before[0], cr, before[1], lf, before[2]))
    assert cr == before[1], "CR count moved"
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
