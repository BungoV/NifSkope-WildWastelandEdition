"""BAKEPERF1: the CLI's region chunk loop becomes the shared parallel pass.

Replaces nifcli.cpp lines 3505..3594 (1-based, inclusive) -- `int done = 0,
skipped = 0, failed = 0;` through the two closing braces of the `for (cy)
for (cx)` loop -- with a job list, one call to lodgenRunChunkPass(), and a
retire lambda that prints exactly the lines the loop printed, in exactly the
same order.
"""
import sys

P = 'src/nifcli.cpp'
FIRST = 3505      # 1-based, the `int done = ...` line
LAST = 3594       # 1-based, the last `\t\t}` of the cx/cy loops

src = open(P, encoding='utf-8', newline='').read()
lines = src.split('\n')

assert lines[FIRST - 1] == '\t\tint done = 0, skipped = 0, failed = 0;', repr(lines[FIRST - 1])
assert lines[LAST - 1] == '\t\t}', repr(lines[LAST - 1])
assert lines[LAST] == '\t\tif ( lodgenNativeActive() ) {', repr(lines[LAST])

NEW = r'''		int done = 0, skipped = 0, failed = 0;
		QStringList writtenBto;
		/* THE CHUNK QUEUE. The doubly-nested loop that used to stand here is
		 * now lodgenRunChunkPass (lodgenchunkpass.h), shared with the panel and
		 * fanned over lodgenThreadCount() workers. Every line below is printed
		 * from `retire`, which the pass calls on THIS thread in JOB ORDER --
		 * so `writtenBto`, the `[n] <name>` lines and the native accumulator
		 * see the same sequence a one-thread run produces. `--threads 1` is
		 * the exact way back. */
		QVector<LodgenChunkJob> jobs;
		for ( int cy = y0; cy <= region[3]; cy += d )
			for ( int cx = x0; cx <= region[2]; cx += d )
				jobs.append( LodgenChunkJob{ d, cx, cy } );

		LodgenChunkPassOptions pass;
		pass.plugins = file;
		pass.worldspace = worldspace ? worldspace : 0x3CU;
		pass.worldEdid = world.worldspaceEdid();
		pass.wantBtr = true;
		pass.wantBto = true;
		pass.wantTex = !texDir.isEmpty() && !texFromVt;
		pass.terrain = opts;
		pass.cover = coverOpts;
		pass.texDataRoot = dataRoot.isEmpty()
			? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot;
		pass.meshDir = outDir;
		pass.texDir = texDir;
		{
			LodgenObjectOptions oopts;
			oopts.dim = d;
			oopts.identity = identity;
			oopts.bakeAO = bakeAO;
			oopts.cullBuried = cullBuried;
			oopts.cullMargin = cullMargin;
			oopts.aoGrey = aoGrey;
			oopts.aoSkirtCells = aoSkirt;
			oopts.impostorDir = impostors;
			oopts.impostorFromLevel = impostorFromLevel;
			oopts.cardAuxDiv = cardAuxDiv;
			oopts.treesOnly = treesOnly;
			oopts.slotFallback = slotFallback;
			oopts.dataRoot = dataRoot.isEmpty()
				? QStringLiteral( "E:/Tools/Fallout 4/DataUnpacked/Data" ) : dataRoot;
			pass.object = oopts;
		}

		{
			QString passErr;
			const bool passOk = lodgenRunChunkPass( jobs, pass,
				[&]( const LodgenChunkOutcome & r ) {
					if ( pass.wantBtr ) {
						if ( !r.btrBuilt ) {
							if ( r.btrNoLand )
								skipped++;
							else {
								err() << "chunk (" << r.cx << "," << r.cy << "): "
									  << r.btrError << Qt::endl;
								failed++;
							}
						} else if ( !r.btrSaved ) {
							err() << "chunk (" << r.cx << "," << r.cy << "): save failed" << Qt::endl;
							failed++;
						} else {
							done++;
							out() << "[" << done << "] "
								  << QFileInfo( r.btrPath ).fileName() << Qt::endl;
							out().flush();
							if ( !r.texError.isEmpty() )
								err() << "texture bake (" << r.cx << "," << r.cy << "): "
									  << r.texError << Qt::endl;
						}
					}
					if ( r.btoBuilt ) {
						if ( r.btoSaved ) {
							done++;
							writtenBto.append( r.btoPath );
							out() << "[" << done << "] "
								  << QFileInfo( r.btoPath ).fileName() << Qt::endl;
							out().flush();
						} else {
							err() << "objects (" << r.cx << "," << r.cy << "): save failed" << Qt::endl;
							failed++;
						}
					}
				},
				std::function<bool()>(), &msMeshes, &msTextures, &passErr );
			if ( !passOk ) {
				err() << "error: " << passErr << Qt::endl;
				return 1;
			}
			out() << "chunk pass: " << lodgenLastPassJobs() << " job(s) over "
				  << lodgenLastPassWorkers() << " worker(s)" << Qt::endl;
			out().flush();
		}'''

out = lines[:FIRST - 1] + NEW.split('\n') + lines[LAST:]
open(P, 'w', encoding='utf-8', newline='').write('\n'.join(out))
print('replaced %d lines with %d' % (LAST - FIRST + 1, len(NEW.split('\n'))))
