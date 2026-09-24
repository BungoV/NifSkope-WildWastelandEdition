"""BAKEPERF1: the panel's chunk-per-tick loop becomes the shared parallel pass.

Three edits to src/lodgenmanager.cpp:

  1. the per-chunk body of step() (lines 2338..2418, 1-based) is deleted --
     step() keeps only its post-queue half, which now runs once;
  2. a new runChunkQueue() is inserted before step();
  3. startChunks() calls runChunkQueue() before the singleShot that used to
     start the per-chunk chain.
"""

P = 'src/lodgenmanager.cpp'
FIRST = 2338      # `const int dim = queue[done].dim;`
LAST = 2418       # `QTimer::singleShot( 0, this, [this]() { step(); } );`

src = open(P, encoding='utf-8', newline='').read()
lines = src.split('\n')

assert lines[FIRST - 1] == '\t\tconst int dim = queue[done].dim;', repr(lines[FIRST - 1])
assert lines[LAST - 1] == '\t\tQTimer::singleShot( 0, this, [this]() { step(); } );', repr(lines[LAST - 1])
assert lines[LAST] == '\t}', repr(lines[LAST])

# ---- 1. drop the per-chunk body -------------------------------------------
lines = lines[:FIRST - 1] + lines[LAST:]

# ---- 2. insert runChunkQueue() before step() ------------------------------
anchor = '\tvoid step()'
i = lines.index(anchor)

QUEUE = r'''	/*! THE CHUNK QUEUE, over the machine.
	 *
	 *  The panel used to build ONE chunk per event-loop tick, so a 3,060-chunk
	 *  Commonwealth ran on one core with fifteen idle. The loop is now
	 *  lodgenRunChunkPass (lodgenchunkpass.h), shared with the command line and
	 *  fanned over lodgenThreadCount() workers.
	 *
	 *  The window stays live because the pass calls `retire` on THIS thread,
	 *  once per chunk, in QUEUE ORDER, and `retire` pumps the event loop -- the
	 *  same thing the pyramid pass already does through vtProgressThunk.
	 *  Cancel still lands between chunks: the workers poll cancelFlag before
	 *  they pick a job up.
	 *
	 *  The live preview is spliced from `retire` too, so the documents arrive
	 *  in chunk order however the workers finish. No NifModel crosses a thread:
	 *  a worker writes the preview copy as a file, translation already applied,
	 *  and the main thread only opens it. */
	void runChunkQueue()
	{
		if ( queue.isEmpty() )
			return;
		QVector<LodgenChunkJob> jobs;
		jobs.reserve( queue.size() );
		for ( const ChunkJob & j : queue )
			jobs.append( LodgenChunkJob{ j.dim, j.cx, j.cy } );

		LodgenChunkPassOptions pass;
		pass.plugins = pluginString();
		pass.worldspace = wsBox->currentData().toUInt();
		pass.worldEdid = world->worldspaceEdid();
		pass.wantBtr = btrCheck->isChecked();
		pass.wantBto = objectPassOn();
		pass.wantTex = btrCheck->isChecked() && texCheck->isChecked() && !vtSuppliesTex;
		pass.meshDir = meshDir;
		pass.texDir = texDir;
		pass.texDataRoot.clear();          // the game's own folders and archives
		pass.cover = coverOptions();
		if ( previewCheck->isChecked() && skope )
			pass.previewDir = QDir::tempPath();

		LodgenTerrainOptions topts;
		topts.water = waterCheck->isChecked();
		topts.targetTrisPerCell = trisSpin->value();
		topts.terrainIdentity = wantTerrainId();
		topts.geomorph = geomorphCheck->isChecked();
		topts.shoreDenser = shoreCheck->isChecked();
		topts.shoreDensity = shoreDensitySpin->value();
		pass.terrain = topts;

		LodgenObjectOptions oopts;
		oopts.identity = wantIdentity();
		oopts.treeSway = oopts.identity && swayCheck->isChecked();
		oopts.objectChannels = oopts.identity && channelsCheck->isChecked();
		oopts.aoSkirtCells = aoSkirtSpin->value();
		oopts.cullBuried = cullCheck->isChecked();
		oopts.cullMargin = float( cullMarginSpin->value() );
		oopts.slotFallback = slotFallbackCheck->isChecked();
		oopts.bakeAO = aoCheck->isChecked();
		oopts.dataRoot.clear();            // the game's own folders and archives
		oopts.impostorDir = impostorEdit->text();
		oopts.impostorFromLevel = impostorLevelBox->currentData().toInt();
		oopts.cardAuxDiv = cardHalfAuxCheck->isChecked() ? 2 : 1;
		oopts.treesOnly = treesOnlyCheck->isChecked();
		pass.object = oopts;

		QString perr;
		lodgenRunChunkPass( jobs, pass,
			[this]( const LodgenChunkOutcome & r ) {
				progress->setFormat( tr( "chunk %1 at (%2,%3) — %v of %m" )
					.arg( r.dim ).arg( r.cx ).arg( r.cy ) );
				const bool okChunk = ( !btrCheck->isChecked() || r.btrBuilt )
					&& ( !objectPassOn() || r.btoBuilt );
				if ( r.btoSaved )
					writtenBto.append( r.btoPath );
				bool added = false;
				if ( skope && previewCheck->isChecked() ) {
					if ( !r.btrPreviewPath.isEmpty() )
						added = skope->addWorkspaceDocumentFromFile( r.btrPreviewPath ) || added;
					if ( !r.btoPreviewPath.isEmpty() )
						added = skope->addWorkspaceDocumentFromFile( r.btoPreviewPath ) || added;
					if ( added && skope->getGLView() && framePending )
						skope->getGLView()->frameAll();
				}
				map->markChunk( r.dim, r.cx, r.cy,
					okChunk ? LodgenProgressMap::Chunk : LodgenProgressMap::Failed );
				done++;
				progress->setValue( done );
				// the window must stay live: this is the only place that pumps
				QCoreApplication::processEvents();
			},
			[this]() { return cancelFlag.load(); },
			&msMeshes, &msTextures, &perr );
		if ( !perr.isEmpty() )
			lastReport = perr;
	}

'''

lines = lines[:i] + QUEUE.split('\n') + lines[i:]

# ---- 3. startChunks() runs the queue before it finishes --------------------
src = '\n'.join(lines)
old = '''		QTimer::singleShot( 0, this, [this]() { step(); } );
	}

	/*! The pyramid's per-row callback'''
new = '''		runChunkQueue();
		QTimer::singleShot( 0, this, [this]() { step(); } );
	}

	/*! The pyramid's per-row callback'''
assert src.count(old) == 1, src.count(old)
src = src.replace(old, new)

open(P, 'w', encoding='utf-8', newline='').write(src)
print('panel patched')
