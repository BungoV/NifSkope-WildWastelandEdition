# Lane BTOFREE1, 2026-09-16 -- patch 2: the command line drops the .BTO under
# the FO4CS target and keeps `--keep-bto` as the exact way back.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:80])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == 0, 'CR appeared in an LF-only file: ' + path
    open(ROOT + path, 'wb').write(nb)
    print('%-24s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))

P = []

# ---- 1. the global, beside the two the ledger lane already put there -------
P.append((
"""static QString gLgIncremental;
static QString gLgSwitchDigest;
""",
"""static QString gLgIncremental;
static QString gLgSwitchDigest;

/*! `--keep-bto` (lane BTOFREE1, 2026-09-16), a global for the same reason the
 *  two above are: cmdLodgen already carries forty-five parameters.
 *
 *  Under the FO4CS target (`--native <dir>`) the `.BTO` chunk files are
 *  scaffolding, not output: the texture arrays, the card arrays, the shape
 *  merge and the far-ring cut read them back, and nothing downstream of the
 *  bake does. Default now builds them in a scratch directory and removes them.
 *  `--keep-bto` is the exact way back -- the chunks land in the mod folder
 *  exactly as they did, byte for byte, which is what the gate pins. It is NOT
 *  in the switch-digest skip list, because it decides what is on disk. */
static bool gLgKeepBto = false;
""",
))

# ---- 2. the argument, beside --native ---------------------------------------
P.append((
"""		else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();
""",
"""		else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();
		else if ( t == QLatin1String( "--keep-bto" ) ) gLgKeepBto = true;
""",
))

# ---- 3. reset it per invocation, beside gLgIncremental.clear() --------------
P.append((
"""	gLgIncremental.clear();
	gLgSwitchDigest = lodgenSwitchDigestOf( a );
""",
"""	gLgIncremental.clear();
	gLgKeepBto = false;
	gLgSwitchDigest = lodgenSwitchDigestOf( a );
""",
))

# ---- 4. help ---------------------------------------------------------------
P.append((
"""		  << "                                          (docs/LODGEN_NATIVE_LODO_LODI.md);\\n"
		  << "                                          the stock .BTO set is unchanged\\n"
""",
"""		  << "                                          (docs/LODGEN_NATIVE_LODO_LODI.md);\\n"
		  << "                                          the .BTO chunks are built in a\\n"
		  << "                                          scratch folder and REMOVED after\\n"
		  << "                                          the arrays, cards, merge and\\n"
		  << "                                          far-ring cut have read them\\n"
		  << "  lodgen ... --native <dir> --keep-bto     the way back: leave the .BTO chunks\\n"
		  << "                                          in the output folder exactly as a\\n"
		  << "                                          bake before 2026-09-16 did. The\\n"
		  << "                                          manifest sidecar is written either\\n"
		  << "                                          way; a bake with no --native is\\n"
		  << "                                          the stock target and is untouched\\n"
""",
))

# ---- 5. the scratch directory, set up beside the pass options ---------------
P.append((
"""		LodgenChunkPassOptions pass;
		pass.plugins = file;
		pass.worldspace = worldspace ? worldspace : 0x3CU;
		pass.worldEdid = world.worldspaceEdid();
		pass.wantBtr = true;
		pass.wantBto = true;
""",
"""		/* ===== THE .BTO SCRATCH FOLDER (lane BTOFREE1, 2026-09-16) =========
		 *
		 * bungo, 2026-09-12 18:3x: "essentially, no legacy vanilla file types
		 * are now used by us or baked in the FO4CS lod bake". The `.BTO` was
		 * the last one left, and it was left because four post-passes read it
		 * back -- not because anything downstream of the bake wants it.
		 *
		 * So under the FO4CS target it is built HERE instead, every read-back
		 * works on it here, and the teardown below removes it. The directory
		 * sits inside the output folder rather than in %TEMP% so that an
		 * interrupted bake leaves its scaffolding where the operator can see
		 * it; a run that finds one from a dead bake removes it first, which
		 * makes that self-healing rather than a second failure. */
		QString btoScratch;
		lodgenClearBtoDisposition();
		if ( !nativeDir.isEmpty() && !gLgKeepBto ) {
			btoScratch = QDir( outDir ).absolutePath() + QStringLiteral( "/lodgen_bto_scratch" );
			QDir( btoScratch ).removeRecursively();
			if ( !QDir().mkpath( btoScratch ) ) {
				err() << "error: cannot create the .BTO scratch folder " << btoScratch
					  << " -- pass --keep-bto to write the chunks into the output folder "
						 "instead" << Qt::endl;
				return 1;
			}
		}

		LodgenChunkPassOptions pass;
		pass.plugins = file;
		pass.worldspace = worldspace ? worldspace : 0x3CU;
		pass.worldEdid = world.worldspaceEdid();
		pass.wantBtr = true;
		pass.wantBto = true;
		pass.btoScratchDir = btoScratch;
""",
))

# ---- 6. the ledger's produced-file list -------------------------------------
P.append((
"""						QStringList & pf = producedFiles[QString( "%1,%2" ).arg( r.cx ).arg( r.cy )];
						for ( const QString & p : { r.btrPath, r.btoPath, r.manifestPath } )
							if ( !p.isEmpty() && QFileInfo::exists( p ) )
								pf.append( p );
""",
"""						QStringList & pf = producedFiles[QString( "%1,%2" ).arg( r.cx ).arg( r.cy )];
						/* WITH A SCRATCH FOLDER the `.BTO` will not survive this
						 * run, so it is not a tracked output and does not go in
						 * the ledger: digesting a file we are about to delete is
						 * exactly the defect the ledger's own comment below
						 * records, and it made every later --incremental run
						 * rebake the whole region. Its manifest DOES survive,
						 * at its final path in the output folder, and the
						 * digest is taken at ledger-write time, after the move.
						 * `.BTR` is unaffected either way. */
						for ( const QString & p : { r.btrPath, r.btoPath, r.manifestPath } ) {
							if ( p.isEmpty() )
								continue;
							if ( !btoScratch.isEmpty() && p == r.btoPath )
								continue;
							if ( !btoScratch.isEmpty() && p == r.manifestPath ) {
								pf.append( outDir + QStringLiteral( "/" )
									+ QFileInfo( r.btoPath ).fileName()
									+ QStringLiteral( ".manifest.txt" ) );
								continue;
							}
							if ( QFileInfo::exists( p ) )
								pf.append( p );
						}
""",
))

# ---- 7. the teardown, after every read-back and before the ledger ----------
P.append((
"""		/* THE LEDGER GOES HERE, LAST, and the reason is a defect this lane
		 * shipped and its own gate caught.""",
"""		/* ===== THE SCRATCH TEARDOWN (lane BTOFREE1, 2026-09-16) ============
		 *
		 * LAST of the object passes and FIRST of the bookkeeping: every
		 * read-back above -- the texture arrays, the atlas, the shape merge,
		 * the far-ring cut and the card arrays -- has had the chunks and their
		 * manifests side by side, exactly as it did when they lived in the
		 * output folder. Now the manifest sidecars move to the output folder
		 * (bungo's open call is to keep them) and the chunks go.
		 *
		 * The count and the bytes are MEASURED here, from the files, not
		 * predicted from `writtenBto.size()`: a census field states what is on
		 * disk or it states nothing (CONSTITUTION 4). */
		if ( !btoScratch.isEmpty() ) {
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
		} else if ( pass.wantBto ) {
			lodgenSetBtoDisposition( QString(), writtenBto.size(), 0, 0 );
		}

		/* THE LEDGER GOES HERE, LAST, and the reason is a defect this lane
		 * shipped and its own gate caught.""",
))

patch('src/nifcli.cpp', P)
print('patch2 ok')
