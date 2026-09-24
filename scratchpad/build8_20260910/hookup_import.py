#!/usr/bin/env python3
"""Lane BUILD8: the CLI hook-up lane HKX5b never wrote.

HKX5b put src/gltfimport.{h,cpp} and src/hkxwrite.{h,cpp} into NifSkope.pro
(applied by BUILD7) but wrote NO route that reaches them: `gltfImportRead`,
`hkxWrite` and lane HKX1's reader are linked into release/NifSkope.exe and
callable, not reachable.  BUILD8's brief requires the round trip to run
"through the built exe (-no-gui commands or the harness hooks, never a
hand-run GUI)", so the exe needs the import half of the pair lane HKX4b's
hookup.py gave the export half.

Three inserts, all in src/nifcli.cpp, anchored on lines that exist exactly
once (the HKX4 markers applied immediately before this script runs):

  gltf-import <in.gltf> -o OUT.hkx [--bones S.hkx] [--fps N] [--source-rate]
              [--root-motion] [--route a|b] [--tsv PATH] [--skeleton NAME]
  hkx-tsv     <in.hkx>  -o OUT.tsv
              lane HKX1's decode as the same 13 columns tests/hkxwrite_dump.cpp
              writes, so the exe's own read of a clip can be diffed against the
              independent Python decoder.

Skill `ww-anchored-hookup`: --check is the default and writes nothing; every
anchor must match exactly once; a `lane BUILD8` marker makes a double-apply
impossible; the CR/LF counts are asserted after (src/nifcli.cpp is LF-only).

Usage:
    python scratchpad/build8_20260910/hookup_import.py            # --check
    python scratchpad/build8_20260910/hookup_import.py --apply
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

MARK = "lane BUILD8"

CLI_INCLUDE_ANCHOR = '#include "gltfexportnif.h"\t\t\t// lane HKX4\n'
CLI_INCLUDE_TEXT = (
    '#include "gltfimport.h"\t\t\t\t// lane BUILD8\n'
    '#include "hkxwrite.h"\t\t\t\t// lane BUILD8\n'
)

CLI_FUNC_ANCHOR = "int usage()\n"
CLI_FUNC_TEXT = '''/*! `gltf-import <in.gltf> -o OUT.hkx [--bones S.hkx] [--fps N] [--source-rate]
 *  [--root-motion] [--route a|b] [--tsv PATH] [--skeleton NAME]` -- lane BUILD8.
 *
 *  The inverse of `gltf`: reads one glTF animation (src/gltfimport.cpp) and
 *  writes it back as a Fallout 4 interleaved .hkx (src/hkxwrite.cpp).
 *  --bones names the .hkx whose hkaSkeleton gives the target bone order; with
 *  none, the glTF's own node names become the bone names and the map is the
 *  identity.  --tsv also dumps the IMPORTED clip, before it is written, in the
 *  13 columns tests/hkxwrite_dump.cpp uses, so the in-memory clip can be
 *  compared without going through the writer at all.
 *  docs/GLTF_IMPORT.md is the contract.
 */
int cmdGltfImport( const QString & file, const QString & outFile, const QString & bonesFile,
				   float fps, bool sourceRate, bool rootMotion, const QString & route,
				   const QString & tsvFile, const QString & skeletonName )
{
	if ( outFile.isEmpty() ) {
		err() << "gltf-import: -o <out.hkx> is required" << Qt::endl;
		return 2;
	}

	GltfImportOptions opt;
	if ( fps > 0.0f )
		opt.targetFps = fps;
	opt.preserveSourceRate = sourceRate;
	opt.extractRootMotion = rootMotion;
	if ( !skeletonName.isEmpty() )
		opt.originalSkeletonName = skeletonName;
	if ( !bonesFile.isEmpty() ) {
		const HkxAnimFile bf = hkxAnimLoad( bonesFile );
		if ( !bf.ok() || bf.skeletons.isEmpty() ) {
			err() << "gltf-import: " << ( bf.ok() ? QStringLiteral( "%1 carries no hkaSkeleton" ).arg( bonesFile )
												  : bf.error ) << Qt::endl;
			return 1;
		}
		opt.skeletonBoneNames = bf.skeletons.first().boneNames;
	}

	HkxAnimClip clip;
	GltfImportReport rep;
	if ( !gltfImportRead( file, opt, clip, rep ) ) {
		err() << "gltf-import: " << rep.error << Qt::endl;
		return 1;
	}
	out() << "read " << file << ": " << rep.summary() << Qt::endl;
	out() << "  container " << rep.container << "; up-axis " << rep.upAxisArm << Qt::endl;
	out() << "  rate " << rep.rateArm << Qt::endl;
	out() << "  mapping " << rep.mappingArm << Qt::endl;
	out() << "  " << clip.numTracks << " tracks x " << clip.numFrames << " frames, "
		  << rep.matched.size() << " matched, " << rep.unmatchedNodes.size()
		  << " nodes unmatched, " << rep.unmatchedBones.size() << " bones undriven, "
		  << rep.staticTracks << " static" << Qt::endl;
	if ( rep.rootMotionExtracted )
		out() << "  root motion from '" << rep.rootMotionNode << "': max |T| "
			  << rep.rootMotionMaxTranslation << ", max yaw " << rep.rootMotionMaxYawDeg
			  << " deg" << Qt::endl;

	if ( !tsvFile.isEmpty() && !cmdHkxTsvWrite( clip, tsvFile ) )
		return 1;

	HkxWriteOptions wopt;
	if ( route == QLatin1String( "a" ) )
		wopt.route = HkxWriteOptions::RouteXmlPack;
	if ( !skeletonName.isEmpty() )
		wopt.skeletonName = skeletonName;
	HkxWriteReport wrep;
	if ( !hkxWrite( clip, outFile, wopt, wrep ) ) {
		err() << "gltf-import: " << wrep.error << Qt::endl;
		return 1;
	}
	out() << "wrote " << outFile << ": " << wrep.summary() << Qt::endl;
	return 0;
}

/*! `hkx-tsv <in.hkx> -o OUT.tsv` -- lane BUILD8.
 *
 *  Lane HKX1's decode of a clip, as the 13 columns
 *  `frame track bone tx ty tz qx qy qz qw sx sy sz` with root motion on track
 *  -1, which is what tests/hkxwrite_dump.cpp writes and what
 *  scratchpad/hkx5_20260910/tsvcmp.py reads.  It exists so the round trip can
 *  be measured on the built exe's OWN reader as well as on the independent
 *  Python decoder.
 */
int cmdHkxTsv( const QString & file, const QString & outFile )
{
	if ( outFile.isEmpty() ) {
		err() << "hkx-tsv: -o <out.tsv> is required" << Qt::endl;
		return 2;
	}
	const HkxAnimFile f = hkxAnimLoad( file );
	if ( !f.ok() ) {
		err() << "hkx-tsv: " << f.error << Qt::endl;
		return 1;
	}
	if ( f.clips.isEmpty() ) {
		err() << "hkx-tsv: " << file << " carries no animation" << Qt::endl;
		return 1;
	}
	const HkxAnimClip & c = f.clips.first();
	if ( !cmdHkxTsvWrite( c, outFile ) )
		return 1;
	out() << "wrote " << outFile << ": " << c.numFrames << " frames x " << c.numTracks
		  << " tracks, " << c.rootMotion.size() << " root-motion samples, frameDuration "
		  << c.frameDuration << Qt::endl;
	return 0;
}

'''

# cmdHkxTsvWrite is used by cmdGltfImport, so it must be DEFINED before it.
CLI_TSVWRITE_ANCHOR = '/*! `gltf <file.nif> -o out.gltf [--clip C.hkx] [--bones skeleton.hkx]\n'
CLI_TSVWRITE_TEXT = '''/*! The clip TSV both BUILD8 commands write: one row per frame per track,
 *  `frame track bone tx ty tz qx qy qz qw sx sy sz`, root motion as track -1
 *  with `tx ty tz yaw`.  Nine significant digits, the same as
 *  tests/hkxwrite_dump.cpp -- lane BUILD8.
 */
bool cmdHkxTsvWrite( const HkxAnimClip & c, const QString & out )
{
	QFile fo( out );
	if ( !fo.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		err() << "cannot write " << out << Qt::endl;
		return false;
	}
	QTextStream ts( &fo );
	ts.setRealNumberPrecision( 9 );
	ts << "# frame\\ttrack\\tbone\\ttx\\tty\\ttz\\tqx\\tqy\\tqz\\tqw\\tsx\\tsy\\tsz\\n";
	for ( int fr = 0; fr < c.numFrames; fr++ ) {
		for ( int t = 0; t < c.numTracks; t++ ) {
			const HkxTransform & x = c.frames[fr][t];
			ts << fr << '\\t' << t << '\\t' << ( t < c.trackToBone.size() ? c.trackToBone[t] : t );
			const float v[10] = { x.translation[0], x.translation[1], x.translation[2],
				x.rotation[1], x.rotation[2], x.rotation[3], x.rotation[0],
				x.scale[0], x.scale[1], x.scale[2] };
			for ( float e : v )
				ts << '\\t' << QString::number( double( e ), 'g', 9 );
			ts << '\\n';
		}
	}
	for ( int fr = 0; fr < c.rootMotion.size(); fr++ ) {
		const HkxRootMotion & r = c.rootMotion[fr];
		ts << fr << "\\t-1\\t-1";
		const float v[4] = { r.translation[0], r.translation[1], r.translation[2], r.yaw };
		for ( float e : v )
			ts << '\\t' << QString::number( double( e ), 'g', 9 );
		ts << '\\n';
	}
	ts.flush();
	return fo.error() == QFile::NoError;
}

'''

CLI_USAGE_ANCHOR = (
    '\t\t  << "                                          bone names the tracks are read by\\n"\n'
)
CLI_USAGE_TEXT = (
    '\t\t  << "  gltf-import <in.gltf> -o OUT.hkx [--bones S.hkx] [--fps N] [--source-rate]\\n"\n'
    '\t\t  << "                                          the inverse: one glTF animation\\n"\n'
    '\t\t  << "                                          back to a Fallout 4 .hkx.\\n"\n'
    '\t\t  << "                                          --route a packs through HKXPACK,\\n"\n'
    '\t\t  << "                                          b (default) emits the packfile;\\n"\n'
    '\t\t  << "                                          --tsv also dumps the clip it read\\n"\n'
    '\t\t  << "  hkx-tsv <in.hkx> -o OUT.tsv             decode a clip to frame/track rows\\n"\n'
)

CLI_VARS_ANCHOR = "\tbool gltfRootMotion = false;\t\t\t// lane HKX4\n"
CLI_VARS_TEXT = (
    "\tQString gltfRoute, gltfTsv, gltfSkeletonName;\t// lane BUILD8\n"
    "\tfloat gltfFps = 0.0f;\t\t\t\t\t\t// lane BUILD8\n"
    "\tbool gltfSourceRate = false;\t\t\t\t// lane BUILD8\n"
)

CLI_OPTS_ANCHOR = '\t\telse if ( t == QLatin1String( "--root-motion" ) ) gltfRootMotion = true;\t// lane HKX4\n'
CLI_OPTS_TEXT = (
    '\t\telse if ( t == QLatin1String( "--fps" ) ) gltfFps = next().toFloat();\t// lane BUILD8\n'
    '\t\telse if ( t == QLatin1String( "--source-rate" ) ) gltfSourceRate = true;\t// lane BUILD8\n'
    '\t\telse if ( t == QLatin1String( "--route" ) ) gltfRoute = next();\t\t// lane BUILD8\n'
    '\t\telse if ( t == QLatin1String( "--tsv" ) ) gltfTsv = next();\t\t\t// lane BUILD8\n'
    '\t\telse if ( t == QLatin1String( "--skeleton-name" ) ) gltfSkeletonName = next();\t// lane BUILD8\n'
)

CLI_DISPATCH_ANCHOR = (
    '\telse if ( cmd == QLatin1String( "gltf" ) )\t\t\t// lane HKX4\n'
    '\t\trc = cmdGltf( file, outFile, gltfClip, gltfBones, gltfRootMotion );\n'
)
CLI_DISPATCH_TEXT = (
    '\telse if ( cmd == QLatin1String( "gltf-import" ) )\t\t// lane BUILD8\n'
    '\t\trc = cmdGltfImport( file, outFile, gltfBones, gltfFps, gltfSourceRate,\n'
    '\t\t\t\t\t\t\tgltfRootMotion, gltfRoute, gltfTsv, gltfSkeletonName );\n'
    '\telse if ( cmd == QLatin1String( "hkx-tsv" ) )\t\t\t// lane BUILD8\n'
    '\t\trc = cmdHkxTsv( file, outFile );\n'
)

EDITS = [
    ("src/nifcli.cpp", "after", CLI_INCLUDE_ANCHOR, CLI_INCLUDE_TEXT),
    ("src/nifcli.cpp", "before", CLI_TSVWRITE_ANCHOR, CLI_TSVWRITE_TEXT),
    ("src/nifcli.cpp", "before", CLI_FUNC_ANCHOR, CLI_FUNC_TEXT),
    ("src/nifcli.cpp", "after", CLI_USAGE_ANCHOR, CLI_USAGE_TEXT),
    ("src/nifcli.cpp", "after", CLI_VARS_ANCHOR, CLI_VARS_TEXT),
    ("src/nifcli.cpp", "after", CLI_OPTS_ANCHOR, CLI_OPTS_TEXT),
    ("src/nifcli.cpp", "after", CLI_DISPATCH_ANCHOR, CLI_DISPATCH_TEXT),
]


def main(argv):
    apply_it = "--apply" in argv
    files = {}
    for rel, _m, _a, _t in EDITS:
        if rel not in files:
            with open(os.path.join(REPO, rel), "rb") as fh:
                files[rel] = fh.read()
    before = {rel: (len(b), b.count(b"\r"), b.count(b"\n")) for rel, b in files.items()}

    ok = True
    already = 0
    for rel, mode, anchor, text in EDITS:
        raw = files[rel]
        a = anchor.encode("utf-8")
        n = raw.count(a)
        # the marker decides applied-or-not; the anchor always still matches
        probe = text.encode("utf-8").split(b"\n")[0][:60]
        here = probe in raw
        print("%-18s %-6s anchor x%d  %-9s  %s"
              % (rel, mode, n, "PRESENT" if here else "absent",
                 repr(anchor[:56])))
        if here:
            already += 1
        if n != 1:
            ok = False
    print("\n%d of %d anchors match exactly once; %d insertions already present"
          % (sum(1 for rel, _m, an, _t in EDITS
                 if files[rel].count(an.encode("utf-8")) == 1), len(EDITS), already))
    for rel, (nb, cr, lf) in sorted(before.items()):
        print("  %-18s %7d bytes, CR %d, LF %d" % (rel, nb, cr, lf))

    if not apply_it:
        print("\n--check only: nothing written. Re-run with --apply.")
        return 0 if ok else 1
    if not ok:
        print("\nREFUSED: an anchor does not match exactly once.")
        return 1
    if already:
        print("\nREFUSED: %d insertions carry the %r marker already." % (already, MARK))
        return 1

    for rel, mode, anchor, text in EDITS:
        raw = files[rel]
        a, t = anchor.encode("utf-8"), text.encode("utf-8")
        assert raw.count(a) == 1, (rel, anchor[:40])
        files[rel] = raw.replace(a, a + t) if mode == "after" else raw.replace(a, t + a)
    for rel, raw in files.items():
        with open(os.path.join(REPO, rel), "wb") as fh:
            fh.write(raw)
        nb, cr, lf = len(raw), raw.count(b"\r"), raw.count(b"\n")
        was = before[rel]
        print("wrote %-18s %7d bytes (%+d), CR %d (was %d), LF %d (was %d)"
              % (rel, nb, nb - was[0], cr, was[1], lf, was[2]))
        assert cr == was[1], "CR count moved -- the inserted text does not match this file's line endings"
    print("\nNext: qmake BEFORE make; nifcli.o must name gltfimport.h and hkxwrite.h.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
