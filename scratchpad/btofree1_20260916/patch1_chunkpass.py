# Lane BTOFREE1, 2026-09-16 -- patch 1: the chunk pass learns a .BTO scratch dir.
#
# Written as a FILE and run, never a heredoc: MISTAKES.md 2026-09-16 14:0x and
# 16:2x are both the same heredoc-eats-backslashes trap.
import io, sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

def patch(path, pairs, check_lf=True):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:70])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    if check_lf:
        assert nb.count(b'\r') == 0, 'CR appeared in an LF-only file: ' + path
    open(ROOT + path, 'wb').write(nb)
    print('%-32s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))

# ---------------------------------------------------------------- header ----
H_OLD = """	QString texDataRoot;            //!< the data root the TEXTURE bake takes
	QString meshDir;                //!< where the .BTR/.BTO land
"""
H_NEW = """	QString texDataRoot;            //!< the data root the TEXTURE bake takes
	QString meshDir;                //!< where the .BTR/.BTO land
	/*! WHERE THE `.BTO` IS BUILT WHEN IT IS NOT AN OUTPUT (lane BTOFREE1,
	 *  2026-09-16). Empty = today's behaviour exactly: the chunk and its
	 *  manifest sidecar are written into `meshDir` and stay there. Non-empty =
	 *  the FO4CS target, where the `.BTO` is scaffolding rather than a product:
	 *  the chunk AND its manifest are built here instead, every read-back (the
	 *  texture arrays, the card arrays, the shape merge, the far-ring cut)
	 *  works on them here, and the driver moves the manifests into `meshDir`
	 *  and removes this directory when the post-passes are done.
	 *
	 *  The manifest travels WITH the chunk on purpose: every one of those
	 *  passes opens `<btoPath>.manifest.txt` beside the file it is rewriting
	 *  (`lodgen.cpp` 4836 / 5132 / 12439 / 12869 / 13235 / 13487), so a
	 *  manifest left behind in `meshDir` would be the one they never amended.
	 *  `.BTR` is untouched by this and still lands in `meshDir`. */
	QString btoScratchDir;
"""

C_OLD = """	if ( opts.wantBto ) {
		NifModel nif;
		QString manifest, cerr;
		QElapsedTimer tObj;
		tObj.start();
		out.btoBuilt = lodgenBuildObjectChunk( &nif, *w.world, job.cx, job.cy, object, &manifest, &cerr );
		w.msMeshes += tObj.elapsed();
		if ( !out.btoBuilt ) {
			out.btoError = cerr;
		} else {
			const QString path = opts.meshDir + QStringLiteral( "/" ) + stem + QStringLiteral( ".BTO" );
"""
C_NEW = """	if ( opts.wantBto ) {
		NifModel nif;
		QString manifest, cerr;
		QElapsedTimer tObj;
		tObj.start();
		out.btoBuilt = lodgenBuildObjectChunk( &nif, *w.world, job.cx, job.cy, object, &manifest, &cerr );
		w.msMeshes += tObj.elapsed();
		if ( !out.btoBuilt ) {
			out.btoError = cerr;
		} else {
			/* The scratch directory when the `.BTO` is scaffolding, `meshDir`
			 * when it is an output (lane BTOFREE1). One line, and it decides
			 * the manifest with it, because every post-pass opens the manifest
			 * from the chunk's own path. */
			const QString btoDir = opts.btoScratchDir.isEmpty() ? opts.meshDir : opts.btoScratchDir;
			const QString path = btoDir + QStringLiteral( "/" ) + stem + QStringLiteral( ".BTO" );
"""

# the census segment: three statics, one setter, one reader
CEN_OLD = """QString lodgenBakeCensusLine()
{"""
CEN_NEW = """/* THE `.BTO` DISPOSITION (lane BTOFREE1, 2026-09-16). Statics because the
 * census line is one shared formatter with no argument list, exactly as the
 * thread counts above it are. `g_btoState` is a TRI-STATE and its default
 * accuses its own plumbing: a run that never entered the object pass reads
 * `n/a`, not `0 dropped`, so "nothing was dropped" and "nothing was asked"
 * can never be read as the same sentence (CONSTITUTION 4, the three rules of
 * 2026-09-04 21:33). */
static QString g_btoWhere;                 // the scratch directory, or empty
static int     g_btoState   = 0;           // 0 n/a, 1 kept in the mod folder, 2 dropped
static int     g_btoChunks  = 0;           // how many .BTO files the run built
static int     g_btoDropped = 0;           // how many of them were removed
static qint64  g_btoFreed   = 0;           // their total size in bytes

void lodgenSetBtoDisposition( const QString & scratchDir, int built, int dropped, qint64 bytesFreed )
{
	g_btoWhere   = scratchDir;
	g_btoState   = scratchDir.isEmpty() ? 1 : 2;
	g_btoChunks  = built;
	g_btoDropped = dropped;
	g_btoFreed   = bytesFreed;
}

void lodgenClearBtoDisposition()
{
	g_btoWhere.clear();
	g_btoState = 0;
	g_btoChunks = g_btoDropped = 0;
	g_btoFreed = 0;
}

QString lodgenBtoDispositionLine()
{
	if ( g_btoState == 0 )
		return QStringLiteral( "bto n/a (no object pass ran)" );
	if ( g_btoState == 1 )
		return QStringLiteral( "bto built in the mod folder, %1 chunk(s), 0 dropped, 0 bytes freed" )
			.arg( g_btoChunks );
	return QStringLiteral( "bto built in scratch %1, %2 chunk(s), %3 dropped, %4 bytes freed" )
		.arg( g_btoWhere ).arg( g_btoChunks ).arg( g_btoDropped ).arg( g_btoFreed );
}

QString lodgenBakeCensusLine()
{"""

CEN2_OLD = """		.arg( g_lastJobs ).arg( g_lastWorkers ).arg( lodgenPeakWorkingSetLine() );
}"""
CEN2_NEW = """		.arg( g_lastJobs ).arg( g_lastWorkers ).arg( lodgenPeakWorkingSetLine() )
		+ QStringLiteral( ", " ) + lodgenBtoDispositionLine();
}"""

CENH_OLD = """QString lodgenBakeCensusLine();

#endif // LODGENCHUNKPASS_H"""
CENH_NEW = """QString lodgenBakeCensusLine();

/*! THE `.BTO` DISPOSITION, said out loud in the census (lane BTOFREE1,
 *  2026-09-16). Under the FO4CS target the `.BTO` chunks are scaffolding and
 *  are removed when the post-passes have read them; a bake that hides that
 *  would be a bake whose output nobody can account for.
 *
 *      bto built in scratch E:/.../lodgen_bto_scratch, 9 chunk(s), 9 dropped,
 *      4485434 bytes freed
 *      bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed
 *      bto n/a (no object pass ran)
 *
 *  `lodgenSetBtoDisposition` with an EMPTY scratch directory says "kept";
 *  with one, "dropped". Never called at all = `n/a`, which is a default that
 *  accuses its own plumbing rather than reporting a comfortable zero. */
void lodgenSetBtoDisposition( const QString & scratchDir, int built, int dropped, qint64 bytesFreed );
void lodgenClearBtoDisposition();
QString lodgenBtoDispositionLine();

#endif // LODGENCHUNKPASS_H"""

patch('src/lodgenchunkpass.h', [(H_OLD, H_NEW), (CENH_OLD, CENH_NEW)])
patch('src/lodgenchunkpass.cpp', [(C_OLD, C_NEW), (CEN_OLD, CEN_NEW), (CEN2_OLD, CEN2_NEW)])
print('patch1 ok')
