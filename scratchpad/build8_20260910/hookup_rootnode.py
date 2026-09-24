#!/usr/bin/env python3
"""Lane BUILD8, second pass: expose --root-node on the `gltf-import` command.

MEASURED CAUSE, 2026-09-10 14:4x.  `gltf ... --root-motion` composes the clip's
travel onto the node of the ROOT BONE ("Root"), but the glTF's only scene root
once the up-axis node is consumed is the NIF's own root NiNode, "skeleton.nif",
which reaches no bone.  So `gltf-import --root-motion` with no node named
refused, correctly and by name:

    gltf-import: root motion was asked for from 'skeleton.nif',
                 which has no track (it reached no bone)

`GltfImportOptions::rootNodeName` already answers this (lane HKX5b) and the
standalone driver already exposes it as `--root-node`; only the CLI command
lane BUILD8 wrote an hour earlier did not.  This adds the flag and nothing
else -- no behaviour of the importer changes, and with the flag absent the
command behaves exactly as before.

Skill `ww-anchored-hookup`: --check writes nothing, every anchor exactly once,
CR/LF asserted after (src/nifcli.cpp is LF-only).
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

SIG_ANCHOR = (
    "int cmdGltfImport( const QString & file, const QString & outFile, const QString & bonesFile,\n"
    "\t\t\t\t   float fps, bool sourceRate, bool rootMotion, const QString & route,\n"
    "\t\t\t\t   const QString & tsvFile, const QString & skeletonName )\n"
)
SIG_TEXT = (
    "int cmdGltfImport( const QString & file, const QString & outFile, const QString & bonesFile,\n"
    "\t\t\t\t   float fps, bool sourceRate, bool rootMotion, const QString & route,\n"
    "\t\t\t\t   const QString & tsvFile, const QString & skeletonName,\n"
    "\t\t\t\t   const QString & rootNode )\t\t\t// lane BUILD8: --root-node\n"
)

BODY_ANCHOR = "\topt.extractRootMotion = rootMotion;\n"
BODY_TEXT = (
    "\t// The clip's travel is composed onto the ROOT BONE's node on export, not\n"
    "\t// onto the NIF's root NiNode -- which is what the single-scene-root rule\n"
    "\t// would pick, and which reaches no bone.  Name it: --root-node Root.\n"
    "\tif ( !rootNode.isEmpty() )\n"
    "\t\topt.rootNodeName = rootNode;\n"
)

VARS_ANCHOR = "\tQString gltfRoute, gltfTsv, gltfSkeletonName;\t// lane BUILD8\n"
VARS_TEXT = "\tQString gltfRootNode;\t\t\t\t\t\t// lane BUILD8\n"

OPTS_ANCHOR = '\t\telse if ( t == QLatin1String( "--skeleton-name" ) ) gltfSkeletonName = next();\t// lane BUILD8\n'
OPTS_TEXT = '\t\telse if ( t == QLatin1String( "--root-node" ) ) gltfRootNode = next();\t// lane BUILD8\n'

DISPATCH_ANCHOR = (
    '\t\trc = cmdGltfImport( file, outFile, gltfBones, gltfFps, gltfSourceRate,\n'
    '\t\t\t\t\t\t\tgltfRootMotion, gltfRoute, gltfTsv, gltfSkeletonName );\n'
)
DISPATCH_TEXT = (
    '\t\trc = cmdGltfImport( file, outFile, gltfBones, gltfFps, gltfSourceRate,\n'
    '\t\t\t\t\t\t\tgltfRootMotion, gltfRoute, gltfTsv, gltfSkeletonName,\n'
    '\t\t\t\t\t\t\tgltfRootNode );\n'
)

USAGE_ANCHOR = '\t\t  << "                                          --tsv also dumps the clip it read\\n"\n'
USAGE_TEXT = (
    '\t\t  << "                                          [--root-motion --root-node Root]\\n"\n'
    '\t\t  << "                                          lifts the travel off that node\\n"\n'
)

EDITS = [
    ("src/nifcli.cpp", "replace", SIG_ANCHOR, SIG_TEXT),
    ("src/nifcli.cpp", "after", BODY_ANCHOR, BODY_TEXT),
    ("src/nifcli.cpp", "after", VARS_ANCHOR, VARS_TEXT),
    ("src/nifcli.cpp", "after", OPTS_ANCHOR, OPTS_TEXT),
    ("src/nifcli.cpp", "replace", DISPATCH_ANCHOR, DISPATCH_TEXT),
    ("src/nifcli.cpp", "after", USAGE_ANCHOR, USAGE_TEXT),
]

MARKER = b"--root-node"


def main(argv):
    apply_it = "--apply" in argv
    with open(os.path.join(REPO, "src/nifcli.cpp"), "rb") as fh:
        raw = fh.read()
    before = (len(raw), raw.count(b"\r"), raw.count(b"\n"))
    ok = True
    for _rel, mode, anchor, _text in EDITS:
        n = raw.count(anchor.encode("utf-8"))
        print("%-8s anchor x%d  %s" % (mode, n, repr(anchor[:56])))
        if n != 1:
            ok = False
    already = raw.count(MARKER)
    print("\nanchors ok: %s; %r occurrences already: %d" % (ok, MARKER.decode(), already))
    print("  src/nifcli.cpp %d bytes, CR %d, LF %d" % before)
    if not apply_it:
        print("\n--check only: nothing written.")
        return 0 if ok else 1
    if not ok:
        print("\nREFUSED: an anchor does not match exactly once.")
        return 1
    if already:
        print("\nREFUSED: the flag is already there.")
        return 1
    for _rel, mode, anchor, text in EDITS:
        a, t = anchor.encode("utf-8"), text.encode("utf-8")
        assert raw.count(a) == 1, anchor[:40]
        if mode == "replace":
            raw = raw.replace(a, t)
        elif mode == "after":
            raw = raw.replace(a, a + t)
        else:
            raw = raw.replace(a, t + a)
    with open(os.path.join(REPO, "src/nifcli.cpp"), "wb") as fh:
        fh.write(raw)
    nb, cr, lf = len(raw), raw.count(b"\r"), raw.count(b"\n")
    print("wrote src/nifcli.cpp %d bytes (%+d), CR %d (was %d), LF %d (was %d)"
          % (nb, nb - before[0], cr, before[1], lf, before[2]))
    assert cr == before[1], "CR count moved"
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
