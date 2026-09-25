"""BAKE1: `--fo4cs-one-root` (opt-in, command line only). Under --native, the three FO4CSLOD-rooted families the
region bake roots at --out-dir -- the object/card arrays (Objects/), the chunk manifests and the bake record
(.lodb) -- are rooted at --native instead, beside the .lodl/.lodo/.lodi/.lodt. That is the LOD panel's layout
(one output folder) while the stock .BTR/.DDS set stays in --out-dir (the brief's R2: stock to scratch).
Without the switch nothing moves. Anchors asserted == 1; LF files, CR count asserted unchanged."""
import sys
ROOT = 'E:/Projects/NifskopeWWE-bake1/src/'
CHECK = '--check' in sys.argv

def patch(name, edits):
    path = ROOT + name
    data = open(path, 'rb').read()
    cr0 = data.count(b'\r')
    for old, new in edits:
        n = data.count(old)
        assert n == 1, '%s: anchor matched %d times: %r' % (name, n, old[:70])
        data = data.replace(old, new)
    assert data.count(b'\r') == cr0, name + ': CR count moved'
    if CHECK:
        print(name, 'ok (check only)')
        return
    with open(path, 'wb') as f:
        f.write(data)
    print(name, 'patched')

H = [
(b"""	QString nativeDir;          //!< the FO4CS target's root; empty = the stock target
	QString digestRoot;         //!< the loose root the per-chunk input digest reads
""", b"""	QString nativeDir;          //!< the FO4CS target's root; empty = the stock target
	//! where the record goes when not `outDir` (`--fo4cs-one-root`, lane BAKE1); empty = `outDir`
	QString recordRoot;
	QString digestRoot;         //!< the loose root the per-chunk input digest reads
"""),
]
P = [
(b"""	run.ledgerPath = lodbRecordPath( run.outDir, ws, fo4cs );
""", b"""	run.ledgerPath = lodbRecordPath( run.recordRoot.isEmpty() ? run.outDir : run.recordRoot, ws, fo4cs );
"""),
]
CLI = [
(b"""static bool gLgAllRings = false;
""", b"""static bool gLgAllRings = false;
/*! `--fo4cs-one-root` (lane BAKE1, 2026-09-25): under --native, the arrays,
 *  the chunk manifests and the bake record are rooted at --native, not at
 *  --out-dir -- the panel's one-folder layout with the stock chunks kept
 *  apart. Off = the layout every gate pins. */
static bool gLgOneRoot = false;
"""),
(b"""	gLgAllRings = false;
	gLgKeepBto = false;
""", b"""	gLgAllRings = false;
	gLgOneRoot = false;
	gLgKeepBto = false;
"""),
(b"""		else if ( t == QLatin1String( "--incremental" ) ) gLgIncremental = next();
""", b"""		else if ( t == QLatin1String( "--incremental" ) ) gLgIncremental = next();
		else if ( t == QLatin1String( "--fo4cs-one-root" ) ) gLgOneRoot = true;
"""),
(b"""		const bool fo4csTarget = !nativeDir.isEmpty();
		auto objectsDir = [&]() {
			return fo4csTarget
				? lodgenFo4csWorldDir( outDir, world.worldspaceEdid() ) + QStringLiteral( "/Objects" )
""", b"""		const bool fo4csTarget = !nativeDir.isEmpty();
		const QString fo4csRoot = gLgOneRoot && fo4csTarget ? nativeDir : outDir;
		auto objectsDir = [&]() {
			return fo4csTarget
				? lodgenFo4csWorldDir( fo4csRoot, world.worldspaceEdid() ) + QStringLiteral( "/Objects" )
"""),
(b"""			const QString manifestDir =
				lodgenFo4csWorldDir( outDir, world.worldspaceEdid() );
""", b"""			const QString manifestDir =
				lodgenFo4csWorldDir( gLgOneRoot && !nativeDir.isEmpty() ? nativeDir : outDir,
					world.worldspaceEdid() );
"""),
(b"""		inc.nativeDir = nativeDir;
		inc.digestRoot = pass.texDataRoot;
""", b"""		inc.nativeDir = nativeDir;
		if ( gLgOneRoot && !nativeDir.isEmpty() )
			inc.recordRoot = nativeDir;
		inc.digestRoot = pass.texDataRoot;
"""),
]
patch('lodgenchunkpass.h', H)
patch('lodgenchunkpass.cpp', P)
patch('nifcli.cpp', CLI)
