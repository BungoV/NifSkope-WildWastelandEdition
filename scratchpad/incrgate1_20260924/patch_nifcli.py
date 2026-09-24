"""INCRGATE1 step 2b: nifcli.cpp keeps only calls into the ledger API.

Run AFTER patch_chunkpass.py (which copies the statics out verbatim).
Every cut is located by a start marker and an end marker that each occur once;
the CR count (0) must be unchanged."""
W = 'E:/Projects/NifskopeWWE-incrgate1'
p = W + '/src/nifcli.cpp'
with open(p, 'rb') as f:
    s = f.read().decode('utf-8')
cr0 = s.count('\r')
assert 'lodgenIncrementalBegin' not in s

def one(a, frm=0):
    n = s.count(a)
    assert n == 1, (a[:70], n)
    return s.index(a)

def cut(start, end_marker, repl, include_end=True):
    """replace s[start marker .. end of end marker] with repl"""
    global s
    i = one(start)
    j = s.index(end_marker, i)
    assert s.count(end_marker, i, j + len(end_marker)) == 1
    j = j + len(end_marker) if include_end else j
    s = s[:i] + repl + s[j:]

# (a) lodbRecordPath + lodbFindRecord -> the new file
cut('/*! WHERE THE RECORD GOES, composed in ONE place',
    '/*! `--keep-bto` (lane BTOFREE1, 2026-09-16), a global',
    '/* lodbRecordPath(), lodbFindRecord(), the switch-digest skip lists and\n'
    ' * lodgenSwitchDigestOf() moved to src/lodgenchunkpass.cpp with the rest of the\n'
    ' * ledger (lane INCRGATE1, 2026-09-24), so the panel writes the same record. */\n\n'
    '/*! `--keep-bto` (lane BTOFREE1, 2026-09-16), a global')

# (b) the skip lists and the digest
i = one('/*! Flags whose TOKEN AND VALUE are both dropped from the digest')
j = one('static QString lodgenSwitchDigestOf( const QStringList & a )\n{')
j = s.index('\n}\n', j) + 3
if s[j:j + 1] == '\n':
    j += 1
s = s[:i] + s[j:]

# (c)+(d) the pass is built BEFORE the diff; the incremental block becomes calls
pi = one('\t\tLodgenChunkPassOptions pass;\n\t\tpass.plugins = file;\n')
pj = s.index('\t\t\tpass.object = oopts;\n\t\t}\n', pi) + len('\t\t\tpass.object = oopts;\n\t\t}\n')
passblock = s[pi:pj]
scr = '\t\tpass.btoScratchDir = btoScratch;\n'
assert passblock.count(scr) == 1
passblock = passblock.replace(scr, '')
s = s[:pi] + scr + s[pj:]

inc_new = r'''		/* ===== INCREMENTAL REGENERATION AND THE BAKE RECORD ===============
		 *
		 * Lane INCR1 (2026-09-12) put the filter HERE, on the job list, and
		 * nowhere else: everything downstream -- the retire callback,
		 * `writtenBto`, the `[n]` lines, the native accumulator -- consumes
		 * the pass IN JOB ORDER, so a filtered list is still in job order and
		 * the bytes of the chunks that DO run cannot depend on which of their
		 * neighbours ran beside them.
		 *
		 * Lane INCRGATE1 (2026-09-24) moved the ledger itself -- the diff, the
		 * refusals, the `.lodj` cache hooks and the record -- into
		 * `src/lodgenchunkpass.cpp`, so the LOD Generation panel runs the same
		 * code. The words printed here did not change.
		 *
		 * The pass is built FIRST because the IDENTITY WORD is read off it: the
		 * record's `switches` is now the argv digest AND every effective
		 * setting, so a default that moved between two builds refuses an
		 * incremental run instead of keeping yesterday's chunks. */
''' + passblock + r'''		LodgenIdentityExtras idx;
		idx.atlas = atlas;
		idx.arrays = arrays;
		idx.merge = merge;
		idx.atlasBc1 = atlasBc1;
		idx.keepBto = gLgKeepBto;
		idx.texFromVt = texFromVt;
		idx.simplify = simplify;
		idx.vt = vtOpts;
		idx.vtBtr = vtBtr;
		idx.nativeLadder = nativeLadder;
		idx.nativeOccluders = nativeOccluders;
		idx.libraryNear = libraryNear;
		idx.ladderFoliage = ladderFoliage;
		idx.silhouetteMin = silhouetteMin;
		idx.placementAo = placementAo;
		idx.vertexAo = vertexAo;
		idx.lodiV7 = lodiV7;
		idx.scrappable = scrappable;
		idx.identityJoinLegacy = identityJoinLegacy;
		idx.identityJoinGap = identityJoinGap;
		idx.aggregate = aggregate;
		idx.aggMin = aggMin;
		idx.aggTile = aggTile;
		idx.aggViews = aggViews;
		const QStringList idDump = lodgenIdentityDump( pass, idx );
		const QString idWord = lodgenIdentityWord( idDump );
		out() << "identity: " << idWord << ", " << idDump.size() << " setting(s)" << Qt::endl;
		{
			// WW_LODGEN_IDENTITY_DUMP=<file>: the lines the word is hashed from, for a gate to diff
			const QString dumpTo = qEnvironmentVariable( "WW_LODGEN_IDENTITY_DUMP" );
			QFile df( dumpTo );
			if ( !dumpTo.isEmpty() && df.open( QIODevice::WriteOnly ) )
				df.write( ( idDump.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8() );
		}

		LodgenIncrementalRun inc;
		inc.fromDir = gLgIncremental;
		inc.requireRecord = true;
		inc.outDir = outDir;
		inc.nativeDir = nativeDir;
		inc.digestRoot = pass.texDataRoot;
		inc.worldspace = pass.worldspace;
		inc.dim = d;
		for ( int k = 0; k < 4; k++ )
			inc.region[k] = region[k];
		inc.switches = lodgenSwitchesWithIdentity( gLgSwitchDigest, idWord );
		inc.regionProducts = atlas || arrays || !impostors.isEmpty();
		inc.nativeCache = gLgNativeCache;
		inc.warn = []( const QString & w ) { err() << w << Qt::endl; };
		{
			QString incCensus, incDetail;
			QStringList incReasons;
			const LodgenIncrementalVerdict v =
				lodgenIncrementalBegin( inc, world, jobs, &incCensus, &incReasons, &incDetail );
			if ( v != LodgenIncrementalVerdict::Go ) {
				for ( const QString & l : lodgenIncrementalRefusal( v, inc, incDetail ) )
					err() << l << Qt::endl;
				return 1;
			}
			if ( !incCensus.isEmpty() ) {
				censusOut( incCensus );
				for ( const QString & r : incReasons )
					out() << r << Qt::endl;
				out().flush();
			}
		}
'''
cut('\t\t/* ===== INCREMENTAL REGENERATION (lane INCR1, 2026-09-12) ==========\n',
    '\t\t\tjobs = kept;\n\t\t\tincremental = true;\n\t\t}\n', inc_new)

# (e) the cache arm
cut('\t\t/* What each retired job actually put on disk, for the ledger. Filled in\n',
    '\t\t\t\t\tlodjPlacements += lodgenNativeCacheLastPlacements();\n\t\t\t\t};\n\t\t\t}\n\t\t}\n',
    '\t\t/* THE PER-CHUNK NATIVE CACHE (lane INCR1, 2026-09-17): written on every\n'
    '\t\t * `--native` bake unless `--no-native-cache`, replayed for the chunks an\n'
    '\t\t * incremental run skips. The hooks live in lodgenchunkpass.cpp. */\n'
    '\t\tlodgenIncrementalArmCache( inc, world.worldspaceEdid(), pass );\n')

# (f) the retire hook
cut('\t\t\t\t\t{\n\t\t\t\t\t\tQStringList & pf = producedFiles[',
    '\t\t\t\t\t\t\t\t\tpf.append( sheetBase + sfx );\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n',
    '\t\t\t\t\tlodgenIncrementalNoteRetired( inc, pass, r );\n')

# (g) the cache census, its refusals and the reuse offer
cut('\t\t\tif ( !nativeDir.isEmpty() && gLgNativeCache ) {\n\t\t\t\tcensusOut( QString( "native cache: ',
    '\t\t\t\tlodgenNativeOfferLibraryReuse( offer );\n\t\t\t}\n',
    '\t\t\t{\n'
    '\t\t\t\tconst QString cc = lodgenIncrementalCacheCensus( inc );\n'
    '\t\t\t\tif ( !cc.isEmpty() ) {\n'
    '\t\t\t\t\tcensusOut( cc );\n'
    '\t\t\t\t\tout().flush();\n'
    '\t\t\t\t}\n'
    '\t\t\t\tconst QStringList refused = lodgenIncrementalCacheRefusal( inc );\n'
    '\t\t\t\tif ( !refused.isEmpty() ) {\n'
    '\t\t\t\t\tfor ( const QString & l : refused )\n'
    '\t\t\t\t\t\terr() << l << Qt::endl;\n'
    '\t\t\t\t\tlodgenNativeEnd();\n'
    '\t\t\t\t\treturn 1;\n'
    '\t\t\t\t}\n'
    '\t\t\t\tlodgenIncrementalOfferReuse( inc );\n'
    '\t\t\t}\n')

# (h) the record
cut('\t\t{\n\t\t\tLodgenLedger led;\n\t\t\tled.worldspace = worldspace ? worldspace : 0x3CU;\n',
    '\t\t\t\t\t\t.arg( QFileInfo( ledgerPath ).size() ) << Qt::endl;\n\t\t\t\t}\n\t\t\t}\n\t\t\tout().flush();\n\t\t}\n',
    '\t\t{\n'
    '\t\t\tQStringList recWarn;\n'
    '\t\t\tQString recLine;\n'
    '\t\t\tlodgenIncrementalWriteRecord( inc, world, gLgArgv, gLgResourceStack, &recWarn, &recLine );\n'
    '\t\t\tfor ( const QString & w : recWarn )\n'
    '\t\t\t\terr() << w << Qt::endl;\n'
    '\t\t\tif ( !recLine.isEmpty() )\n'
    '\t\t\t\tout() << recLine << Qt::endl;\n'
    '\t\t\tout().flush();\n'
    '\t\t}\n')

for gone in ('prevLedger', 'producedFiles', 'lodjWritten', 'ledgerPath'):
    assert gone not in s, gone
assert s.count('\r') == cr0
with open(p, 'wb') as f:
    f.write(s.encode('utf-8'))
print('nifcli patched', s.count('\n'), 'lines')
