#!/usr/bin/env python3
"""Lane HKX4b: the eleven lines four EXISTING files need, as edits nobody has
applied. Skill `ww-anchored-hookup`.

The lane's own work is in NEW files -- src/gltfexport.{h,cpp},
src/gltfexportnif.{h,cpp}, src/lib/importex/gltfanim.cpp, tests/... -- which
one lane may own. What follows is what the shared files need, and it is NOT
applied: `--check` is the default and writes nothing.

  NifSkope.pro                    3 inserts: the two new src/ pairs in HEADERS
                                  and SOURCES, and gltfanim.cpp beside the
                                  existing importex/gltf.cpp
  src/lib/importex/importex.cpp   2 inserts: the forward declaration and the
                                  menu row ".glTF (skeleton, skin, animation)"
  src/nifcli.cpp                  5 inserts: the include, the cmdGltf()
                                  function, its usage text, its options and
                                  its dispatch line

After applying: qmake BEFORE make (three new sources), then read the
dependency block of each new object back by name -- qmake freezes the
dependency lists when the Makefile is generated (skill
nifskope-ww-resume-pending, section 3).

WHAT THIS DOES NOT DO, and is not authorised to do: wire the Animation
workspace's loaded-clip list to `gltfExportSetClipProvider()`. That list is
lane HKX3's and the seam is declared in src/gltfexportnif.h; until HKX3 calls
the setter, the MENU entry exports the character and says in its own message
box that no animation was written. The CLI does not need it -- it loads the
.hkx itself. See CHANGE_NEEDED.md beside this file.

Usage:
    python scratchpad/hkx4_20260910/hookup.py            # --check, writes nothing
    python scratchpad/hkx4_20260910/hookup.py --apply
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

# A marker every inserted block carries, so "applied or not" is decided by
# looking for it and never by whether the anchor still matches (the anchor is
# the line the text goes AFTER, so it always still matches -- the trap that
# cost lane BUILD5b a build on 2026-09-10).
MARK = "lane HKX4"

PRO_HEADERS_ANCHOR = "\tsrc/hkxanim.h \\\n"
PRO_HEADERS_TEXT = (
    "\tsrc/gltfexport.h \\\n"
    "\tsrc/gltfexportnif.h \\\n"
)

PRO_SOURCES_ANCHOR = "\tsrc/hkxanim.cpp \\\n"
PRO_SOURCES_TEXT = (
    "\tsrc/gltfexport.cpp \\\n"
    "\tsrc/gltfexportnif.cpp \\\n"
)

PRO_IMPORTEX_ANCHOR = "\tsrc/lib/importex/gltf.cpp \\\n"
PRO_IMPORTEX_TEXT = "\tsrc/lib/importex/gltfanim.cpp \\\n"

IMPEX_DECL_ANCHOR = "void importGltf( NifModel* nif, const QModelIndex& index );\n"
IMPEX_DECL_TEXT = (
    "\n"
    "// src/lib/importex/gltfanim.cpp -- the animated export (lane HKX4). It writes\n"
    "// the node tree, the skinned meshes AND the clip loaded in the Animation\n"
    "// workspace; exportGltf above stays the exporter for a static scene, with its\n"
    "// embedded textures, LODs and Starfield. docs/GLTF_INTERCHANGE.md.\n"
    "void exportGltfAnimated( const NifModel* nif, const Scene* scene, const QModelIndex& index );\n"
)

IMPEX_ROW_ANCHOR = '\tImportExportOption{ ".glTF", importGltf, exportGltf, 83 },\n'
IMPEX_ROW_TEXT = (
    '\tImportExportOption{ ".glTF (skeleton, skin, animation)", nullptr, exportGltfAnimated, 130, 130 },'
    '\t// lane HKX4\n'
)

CLI_INCLUDE_ANCHOR = '#include "gl/hknpdecode.h"\n'
CLI_INCLUDE_TEXT = '#include "gltfexportnif.h"\t\t\t// lane HKX4\n'

CLI_FUNC_ANCHOR = "int usage()\n"
CLI_FUNC_TEXT = '''/*! `gltf <file.nif> -o out.gltf [--clip C.hkx] [--bones skeleton.hkx]
 *  [--root-motion]` -- lane HKX4.
 *
 *  Writes out.gltf and out.bin: the NIF's node tree, its skinned shapes and,
 *  when --clip is given, that clip as a glTF animation at its own frame rate.
 *  --bones names the file whose hkaSkeleton gives the tracks their bone names
 *  (a clip usually carries none of its own; use the game's skeleton.hkx).
 *  Without --root-motion the clip plays in place and the travel is recorded
 *  in the animation's extras. docs/GLTF_INTERCHANGE.md is the contract.
 */
int cmdGltf( const QString & file, const QString & outFile, const QString & clipFile,
			 const QString & bonesFile, bool rootMotion )
{
	if ( outFile.isEmpty() ) {
		err() << "gltf: -o <out.gltf> is required" << Qt::endl;
		return 2;
	}
	NifModel nif;
	if ( !loadNif( nif, file ) )
		return 1;

	HkxAnimClip clip;
	QStringList boneNames;
	bool haveClip = false;
	if ( !clipFile.isEmpty() ) {
		const HkxAnimFile cf = hkxAnimLoad( clipFile );
		if ( !cf.ok() ) {
			err() << "gltf: " << cf.error << Qt::endl;
			return 1;
		}
		if ( cf.clips.isEmpty() ) {
			err() << "gltf: " << clipFile << " carries no animation" << Qt::endl;
			return 1;
		}
		clip = cf.clips.first();
		haveClip = true;
		if ( !bonesFile.isEmpty() ) {
			const HkxAnimFile bf = hkxAnimLoad( bonesFile );
			if ( !bf.ok() || bf.skeletons.isEmpty() ) {
				err() << "gltf: " << ( bf.ok() ? QStringLiteral( "%1 carries no hkaSkeleton" ).arg( bonesFile )
											   : bf.error ) << Qt::endl;
				return 1;
			}
			boneNames = bf.skeletons.first().boneNames;
		} else if ( !cf.skeletons.isEmpty() ) {
			boneNames = cf.skeletons.first().boneNames;
		} else {
			err() << "gltf: " << clipFile << " carries no skeleton; pass --bones skeleton.hkx" << Qt::endl;
			return 1;
		}
	}

	GltfExportReport report;
	QString error;
	if ( !gltfExportNifWrite( &nif, QModelIndex(), haveClip ? &clip : nullptr, boneNames,
							  rootMotion, outFile, report, error ) ) {
		err() << "gltf: " << error << Qt::endl;
		return 1;
	}
	out() << "wrote " << outFile << ": " << report.nodes << " nodes, " << report.shapes
		  << " shapes (" << report.skinnedShapes << " skinned), " << report.vertices
		  << " vertices, " << report.triangles << " triangles" << Qt::endl;
	if ( haveClip )
		out() << "  clip '" << clip.name << "': " << clip.numFrames << " frames at "
			  << clip.frameDuration << " s, " << report.tracksMatched << " of "
			  << ( report.tracksMatched + report.tracksUnmatched ) << " tracks matched a node"
			  << Qt::endl;
	for ( const QString & n : report.notes )
		out() << "  " << n << Qt::endl;
	return 0;
}

'''

CLI_USAGE_ANCHOR = (
    '\t\t  << "                                          the .ssf id that addresses it\\n"\n'
)
CLI_USAGE_TEXT = (
    '\t\t  << "  gltf <file> -o OUT.gltf [--clip C.hkx [--bones S.hkx]] [--root-motion]\\n"\n'
    '\t\t  << "                                          glTF 2.0 export: the node tree,\\n"\n'
    '\t\t  << "                                          the skinned shapes and one .hkx\\n"\n'
    '\t\t  << "                                          clip, opened natively by Blender.\\n"\n'
    '\t\t  << "                                          --bones names the skeleton whose\\n"\n'
    '\t\t  << "                                          bone names the tracks are read by\\n"\n'
)

CLI_OPTS_ANCHOR = '\t\telse if ( t == QLatin1String( "--validate" ) ) validateOnly = true;\n'
CLI_OPTS_TEXT = (
    '\t\telse if ( t == QLatin1String( "--clip" ) ) gltfClip = next();\t\t\t// lane HKX4\n'
    '\t\telse if ( t == QLatin1String( "--bones" ) ) gltfBones = next();\t\t// lane HKX4\n'
    '\t\telse if ( t == QLatin1String( "--root-motion" ) ) gltfRootMotion = true;\t// lane HKX4\n'
)

CLI_VARS_ANCHOR = "\tQString saveName, applyName, importOs, exportOs;\n"
CLI_VARS_TEXT = (
    "\tQString gltfClip, gltfBones;\t\t\t// lane HKX4\n"
    "\tbool gltfRootMotion = false;\t\t\t// lane HKX4\n"
)

CLI_DISPATCH_ANCHOR = '\telse if ( cmd == QLatin1String( "segments" ) )\n\t\trc = cmdSegments( file, block );\n'
CLI_DISPATCH_TEXT = (
    '\telse if ( cmd == QLatin1String( "gltf" ) )\t\t\t// lane HKX4\n'
    '\t\trc = cmdGltf( file, outFile, gltfClip, gltfBones, gltfRootMotion );\n'
)

EDITS = [
    ("NifSkope.pro", "after", PRO_HEADERS_ANCHOR, PRO_HEADERS_TEXT),
    ("NifSkope.pro", "after", PRO_SOURCES_ANCHOR, PRO_SOURCES_TEXT),
    ("NifSkope.pro", "after", PRO_IMPORTEX_ANCHOR, PRO_IMPORTEX_TEXT),
    ("src/lib/importex/importex.cpp", "after", IMPEX_DECL_ANCHOR, IMPEX_DECL_TEXT),
    ("src/lib/importex/importex.cpp", "after", IMPEX_ROW_ANCHOR, IMPEX_ROW_TEXT),
    ("src/nifcli.cpp", "after", CLI_INCLUDE_ANCHOR, CLI_INCLUDE_TEXT),
    ("src/nifcli.cpp", "before", CLI_FUNC_ANCHOR, CLI_FUNC_TEXT),
    ("src/nifcli.cpp", "after", CLI_USAGE_ANCHOR, CLI_USAGE_TEXT),
    ("src/nifcli.cpp", "after", CLI_VARS_ANCHOR, CLI_VARS_TEXT),
    ("src/nifcli.cpp", "after", CLI_OPTS_ANCHOR, CLI_OPTS_TEXT),
    ("src/nifcli.cpp", "after", CLI_DISPATCH_ANCHOR, CLI_DISPATCH_TEXT),
]


def main(argv):
    apply_it = "--apply" in argv
    files = {}
    for rel, _mode, _anchor, _text in EDITS:
        if rel not in files:
            with open(os.path.join(REPO, rel), "rb") as fh:
                files[rel] = fh.read()
    before = {rel: (len(b), b.count(b"\r"), b.count(b"\n")) for rel, b in files.items()}

    ok = True
    applied_already = 0
    for rel, mode, anchor, text in EDITS:
        raw = files[rel]
        a = anchor.encode("utf-8")
        n = raw.count(a)
        already = text.strip().encode("utf-8")[:40] in raw
        print("%-30s %-6s anchor x%d  %-9s  %s"
              % (rel, mode, n, "PRESENT" if already else "absent",
                 anchor.strip()[:58].replace("\t", " ")))
        if already:
            applied_already += 1
        if n != 1:
            ok = False
    print("\n%d of %d anchors match exactly once; %d insertions are already present"
          % (sum(1 for rel, _m, an, _t in EDITS if files[rel].count(an.encode('utf-8')) == 1),
             len(EDITS), applied_already))
    for rel, (nb, cr, lf) in sorted(before.items()):
        print("  %-30s %7d bytes, CR %d, LF %d" % (rel, nb, cr, lf))

    if not apply_it:
        print("\n--check only: nothing was written. Re-run with --apply to write.")
        return 0 if ok else 1
    if not ok:
        print("\nREFUSED: an anchor does not match exactly once.")
        return 1
    if applied_already:
        print("\nREFUSED: %d insertions carry the %r marker already." % (applied_already, MARK))
        return 1

    out = {}
    for rel, mode, anchor, text in EDITS:
        raw = files.get(rel)
        a, t = anchor.encode("utf-8"), text.encode("utf-8")
        assert raw.count(a) == 1, rel
        raw = raw.replace(a, a + t) if mode == "after" else raw.replace(a, t + a)
        files[rel] = raw
        out[rel] = raw
    for rel, raw in out.items():
        with open(os.path.join(REPO, rel), "wb") as fh:
            fh.write(raw)
        nb, cr, lf = len(raw), raw.count(b"\r"), raw.count(b"\n")
        was = before[rel]
        print("wrote %-30s %7d bytes (%+d), CR %d (was %d), LF %d (was %d)"
              % (rel, nb, nb - was[0], cr, was[1], lf, was[2]))
        if cr != was[1]:
            print("  WARNING: the CR count moved; the inserted text does not match this file's line endings")
    print("\nNext: qmake BEFORE make (three new sources), then read the dependency block of "
          "GeneratedFiles/.obj/gltfexport.o, gltfexportnif.o and gltfanim.o back by name.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
