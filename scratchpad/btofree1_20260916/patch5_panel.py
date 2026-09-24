# Lane BTOFREE1, 2026-09-16 -- patch 5.
#
#  (a) the scratch teardown becomes ONE function in lodgenchunkpass.cpp, so the
#      panel and the command line cannot drift apart. lodgen_byte_gate.sh
#      compares their output file by file; two copies of this loop would be a
#      byte difference waiting to happen.
#  (b) nifcli.cpp's inline copy (patch 2) calls it instead.
#  (c) the panel grows the row "Keep legacy .BTO chunks", builds the chunks in
#      the scratch folder when it is off under the FO4CS target, and says so.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:90])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s: %d -> %d' % (path, cr0, nb.count(b'\r'))
    open(ROOT + path, 'wb').write(nb)
    print('%-28s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


# ===== (a) the header =======================================================
H = []
H.append((
'''void lodgenSetBtoDisposition( const QString & scratchDir, int built, int dropped, qint64 bytesFreed );''',
'''void lodgenSetBtoDisposition( const QString & scratchDir, int built, int dropped, qint64 bytesFreed );

/*! WHAT THE TEARDOWN OF A `.BTO` SCRATCH FOLDER DID (lane BTOFREE1).
 *
 *  Counted from the files on disk, never predicted from the job list: a census
 *  field states what is there or it states nothing. `warnings` carries the
 *  human sentences for anything that could not be moved or removed -- the bake
 *  itself is complete either way, so none of them is an error. */
struct LodgenBtoScratchResult
{
	int built = 0;              //!< `.BTO` files the run actually produced
	int dropped = 0;            //!< how many of them were removed again
	int manifests = 0;          //!< sidecars moved to the folder they belong in
	qint64 freed = 0;           //!< the bytes those chunks occupied
	QStringList warnings;
};

/*! Move every chunk's manifest sidecar from `scratchDir` into `finalDir`,
 *  delete the chunks, remove the folder, and record the disposition for the
 *  census line. One implementation for the panel and the command line, because
 *  `lodgen_byte_gate.sh` compares what the two of them leave on disk. */
LodgenBtoScratchResult lodgenDropBtoScratch( const QStringList & writtenBto,
	const QString & scratchDir, const QString & finalDir );''',
))
patch('src/lodgenchunkpass.h', H)

# ===== (a) the body =========================================================
C = []
C.append((
'''QString lodgenBtoDispositionLine()''',
'''LodgenBtoScratchResult lodgenDropBtoScratch( const QStringList & writtenBto,
	const QString & scratchDir, const QString & finalDir )
{
	LodgenBtoScratchResult r;
	if ( scratchDir.isEmpty() )
		return r;
	for ( const QString & p : writtenBto ) {
		const QFileInfo fi( p );
		if ( !fi.exists() )
			continue;
		r.built++;
		r.freed += fi.size();
		/* THE SIDECAR TRAVELS WITH THE CHUNK AND STAYS BEHIND IT. Five passes
		 * open `<chunk>.BTO.manifest.txt` beside the chunk they are reading
		 * (lodgen.cpp), so it has to be in the scratch folder while they run;
		 * bungo's call is to keep the manifest, so it has to be in the mod
		 * folder when they are done. Both are true because it moves here. */
		const QString man = p + QStringLiteral( ".manifest.txt" );
		if ( QFileInfo::exists( man ) ) {
			const QString dst = finalDir + QStringLiteral( "/" ) + fi.fileName()
				+ QStringLiteral( ".manifest.txt" );
			QFile::remove( dst );
			if ( QFile::rename( man, dst ) )
				r.manifests++;
			else
				r.warnings.append( QStringLiteral( "warning: %1 could not be moved out of "
					"the scratch folder" ).arg( QFileInfo( man ).fileName() ) );
		}
		if ( QFile::remove( p ) )
			r.dropped++;
	}
	if ( !QDir( scratchDir ).removeRecursively() )
		r.warnings.append( QStringLiteral( "warning: the .BTO scratch folder %1 could not be "
			"removed; the bake itself is complete" ).arg( scratchDir ) );
	lodgenSetBtoDisposition( scratchDir, r.built, r.dropped, r.freed );
	return r;
}

QString lodgenBtoDispositionLine()''',
))
patch('src/lodgenchunkpass.cpp', C)

# ===== (b) the command line calls it ========================================
N = []
N.append((
'''		if ( !btoScratch.isEmpty() ) {
			int built = 0, dropped = 0, manifests = 0;
			qint64 freed = 0;
			QStringList moveFailed;
			for ( const QString & p : writtenBto ) {
				const QFileInfo fi( p );
				if ( !fi.exists() )
					continue;
				built++;
				freed += fi.size();
				const QString man = p + QStringLiteral( ".manifest.txt" );
				if ( QFileInfo::exists( man ) ) {
					const QString dst = outDir + QStringLiteral( "/" ) + fi.fileName()
						+ QStringLiteral( ".manifest.txt" );
					QFile::remove( dst );
					if ( QFile::rename( man, dst ) )
						manifests++;
					else
						moveFailed.append( QFileInfo( man ).fileName() );
				}
				if ( QFile::remove( p ) )
					dropped++;
			}
			if ( !QDir( btoScratch ).removeRecursively() )
				err() << "warning: the .BTO scratch folder " << btoScratch
					  << " could not be removed; the bake itself is complete" << Qt::endl;
			for ( const QString & m : moveFailed )
				err() << "warning: " << m << " could not be moved out of the scratch folder"
					  << Qt::endl;
			lodgenSetBtoDisposition( btoScratch, built, dropped, freed );
			out() << QString( "bto scratch: %1 chunk(s) built in %2, %3 removed, "
							  "%4 manifest sidecar(s) kept, %5 bytes freed" )
				.arg( built ).arg( btoScratch ).arg( dropped ).arg( manifests ).arg( freed )
				<< Qt::endl;
			out().flush();
		} else if ( pass.wantBto ) {''',
'''		if ( !btoScratch.isEmpty() ) {
			const LodgenBtoScratchResult r =
				lodgenDropBtoScratch( writtenBto, btoScratch, outDir );
			for ( const QString & w : r.warnings )
				err() << w << Qt::endl;
			out() << QString( "bto scratch: %1 chunk(s) built in %2, %3 removed, "
							  "%4 manifest sidecar(s) kept, %5 bytes freed" )
				.arg( r.built ).arg( btoScratch ).arg( r.dropped ).arg( r.manifests )
				.arg( r.freed )
				<< Qt::endl;
			out().flush();
		} else if ( pass.wantBto ) {''',
))
patch('src/nifcli.cpp', N)

# ===== (c) the panel ========================================================
M = []

# 1. the row, last in "Object modules" because it is the way BACK from a default
M.append((
'''			xC( f, "LodgenAtlasFormatBox", QStringLiteral( "atlasFormat" ),''',
'''			xB( f, "LodgenKeepBtoCheck", QStringLiteral( "keepBto" ),
				tr( "Keep legacy .BTO chunks" ), false,
				tr( "The FO4CS target builds the .BTO chunk files in a scratch folder and\\n"
					"removes them once the texture arrays, the card arrays, the shape merge\\n"
					"and the far-ring cut have read them: nothing after the bake reads them.\\n"
					"On, they are left in the mod folder as bakes before 2026-09-16 left\\n"
					"them, byte for byte. The manifest sidecar is kept either way, and the\\n"
					"stock engine target is not affected.\\nCommand line: --keep-bto" ) );
			xC( f, "LodgenAtlasFormatBox", QStringLiteral( "atlasFormat" ),''',
))

# 2. it belongs to the FO4CS head
M.append((
'''			showExtra( "nativeLadder", cs );''',
'''			showExtra( "keepBto", cs );
			showExtra( "nativeLadder", cs );''',
))

# 3. the member
M.append((
'''	QString meshDir, texDir, lastReport;''',
'''	QString meshDir, texDir, btoScratch, lastReport;''',
))

# 4. the folder, made where meshDir is made
M.append((
'''		QDir().mkpath( meshDir );
		if ( texCheck->isChecked() && btrCheck->isChecked() )
			QDir().mkpath( texDir );''',
'''		QDir().mkpath( meshDir );
		if ( texCheck->isChecked() && btrCheck->isChecked() )
			QDir().mkpath( texDir );
		/* ===== THE .BTO SCRATCH FOLDER (lane BTOFREE1, 2026-09-16) =========
		 *
		 * bungo, 2026-09-12 18:3x: "essentially, no legacy vanilla file types
		 * are now used by us or baked in the FO4CS lod bake". The `.BTO` was
		 * the last one, and it was last because five passes read it BACK --
		 * not because anything downstream of the bake wants it. So under the
		 * FO4CS target it is built here instead, every read-back works on it
		 * here, and the teardown after the card arrays removes it.
		 *
		 * The folder sits inside the mod folder rather than in %TEMP% so an
		 * interrupted bake leaves its scaffolding where the operator can see
		 * it; a run that finds one left by a dead bake removes it first, which
		 * makes that self-healing instead of a second failure. Same path, same
		 * name and the same teardown function as the command line, because
		 * lodgen_byte_gate.sh compares what the two leave on disk. */
		btoScratch.clear();
		lodgenClearBtoDisposition();
		if ( wantNative() && objectPassOn() && !xb( "keepBto" ) ) {
			btoScratch = outputDir() + QStringLiteral( "/lodgen_bto_scratch" );
			QDir( btoScratch ).removeRecursively();
			if ( !QDir().mkpath( btoScratch ) ) {
				finishAll( tr( "cannot create the .BTO scratch folder %1 \\u2014 tick "
					"\\"Keep legacy .BTO chunks\\" to write the chunks into the mod "
					"folder instead" ).arg( btoScratch ) );
				return;
			}
		}''',
))

# 5. the pass carries it
M.append((
'''		pass.meshDir = meshDir;
		pass.texDir = texDir;''',
'''		pass.meshDir = meshDir;
		pass.btoScratchDir = btoScratch;
		pass.texDir = texDir;''',
))

# 6. the teardown: after every read-back, before the pair
M.append((
'''			/* The native pair, written once the chunk queue has handed the''',
'''			/* ===== THE SCRATCH TEARDOWN (lane BTOFREE1, 2026-09-16) ========
			 *
			 * LAST of the object passes: the texture arrays, the atlas, the
			 * shape merge, the far-ring cut and the card arrays above have all
			 * had the chunks and their manifests side by side, exactly as they
			 * did when the chunks lived in the mod folder. Now the sidecars
			 * move into the mod folder and the chunks go. Before the pair,
			 * because the pair is written from the emitter and not from the
			 * files, and after a CANCEL as well -- a cancelled run must not
			 * leave a scratch folder behind either. */
			if ( !btoScratch.isEmpty() ) {
				const LodgenBtoScratchResult r =
					lodgenDropBtoScratch( writtenBto, btoScratch, meshDir );
				tail += tr( ", %1 .bto chunk(s) dropped from the mod folder (%2 bytes)" )
					.arg( r.dropped ).arg( r.freed );
				for ( const QString & w : r.warnings )
					tail += tr( ", %1" ).arg( w );
				writtenBto.clear();
				btoScratch.clear();
			}
			/* The native pair, written once the chunk queue has handed the''',
))

# 7. the sentence beside Generate stops promising files it no longer writes
M.append((
'''			} else {
				/* Native only. The object pass still walks the chunk queue and
				 * still leaves the `.bto` files behind it: the texture arrays,
				 * the card arrays, the shape merge and the far-ring cut all
				 * read them back, and FO4CS's own Improved LOD module reads
				 * them today. Said out loud rather than left as a surprise --
				 * whether the FO4CS target should stop writing them once the
				 * runtime reads the pair is bungo's call, not this panel's. */
				parts << tr( "%1 object chunks under meshes\\\\terrain\\\\%2, which the pair is built from" )
					.arg( buildQueue().size() ).arg( ws );
			}''',
'''			} else if ( xb( "keepBto" ) ) {
				/* The way back, ticked. The chunks land in the mod folder as
				 * every bake before 2026-09-16 left them. */
				parts << tr( "%1 object chunks under meshes\\\\terrain\\\\%2, kept because "
					"\\"Keep legacy .BTO chunks\\" is on" )
					.arg( buildQueue().size() ).arg( ws );
			} else {
				/* RULED 2026-09-16 (bungo, 2026-09-12 18:3x: "essentially, no
				 * legacy vanilla file types are now used by us or baked in the
				 * FO4CS lod bake"). The object pass still walks the chunk queue
				 * and still builds a `.bto` per chunk, because five passes read
				 * them back -- but it builds them in a scratch folder and drops
				 * them, so the mod folder gets our types and nothing else. The
				 * manifest sidecar stays. Said out loud, because a folder that
				 * loses files at the end of a run should not be a surprise. */
				parts << tr( "%1 object chunks, built in a scratch folder and dropped "
					"(the manifest sidecars stay under meshes\\\\terrain\\\\%2)" )
					.arg( buildQueue().size() ).arg( ws );
			}''',
))

patch('src/lodgenmanager.cpp', M)
print('patch5 ok')

# ===== includes the new helper needs ========================================
patch('src/lodgenchunkpass.h', [(
'''#include <QString>
#include <QVector>''',
'''#include <QString>
#include <QStringList>
#include <QVector>''')])
patch('src/lodgenchunkpass.cpp', [(
'''#include <QElapsedTimer>''',
'''#include <QElapsedTimer>
#include <QFileInfo>''')])
print('includes ok')
