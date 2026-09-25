"""BAKE1: `--dim all` on the command line = the LOD panel's "all rings (4+8+16+32)" -- one region bake, one
native session, every ring, so the far rings' card placements link into the .lodo/.lodi (cardCount > 0) and the
.lodb is written for the whole run. The bake record keys a chunk of another ring by `cx,cy@dim` (a chunk of the
run's own ring keeps `cx,cy`, so a one-ring record is unchanged to the byte). --incremental with --dim all is
refused, as the panel refuses it. Anchors asserted == 1; both files LF only, CR count asserted unchanged."""
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

CLI = [
(b"""static QString gLgIncremental;
static QString gLgSwitchDigest;
""", b"""static QString gLgIncremental;
static QString gLgSwitchDigest;
/*! `--dim all` (lane BAKE1, 2026-09-25): the region bake runs rings 4, 8, 16
 *  and 32 in ONE pass and one native session, the LOD panel's "all rings".
 *  A single ring is what it always was. */
static bool gLgAllRings = false;
"""),
(b"""	gLgIncremental.clear();
	gLgKeepBto = false;
""", b"""	gLgIncremental.clear();
	gLgAllRings = false;
	gLgKeepBto = false;
"""),
(b"""		else if ( t == QLatin1String( "--dim" ) ) lgDim = next().toInt();
""", b"""		else if ( t == QLatin1String( "--dim" ) ) {
			const QString v = next();
			gLgAllRings = v == QLatin1String( "all" );
			lgDim = gLgAllRings ? 4 : v.toInt();
		}
"""),
(b"""		  << "         [--dim 4] --out-dir DIR [--atlas]\\n"
""", b"""		  << "         [--dim 4|8|16|32|all] --out-dir DIR [--atlas]\\n"
		  << "                                          --dim all: rings 4+8+16+32 in one\\n"
		  << "                                          pass, the panel's \\"all rings\\"\\n"
"""),
(b"""		QVector<LodgenChunkJob> jobs;
		for ( int cy = y0; cy <= region[3]; cy += d )
			for ( int cx = x0; cx <= region[2]; cx += d )
				jobs.append( LodgenChunkJob{ d, cx, cy } );
""", b"""		QVector<LodgenChunkJob> jobs;
		for ( int cy = y0; cy <= region[3]; cy += d )
			for ( int cx = x0; cx <= region[2]; cx += d )
				jobs.append( LodgenChunkJob{ d, cx, cy } );
		/* `--dim all`: the wider rings follow the ring-4 jobs, in the panel's
		 * queue order (LodgenManager::buildQueue: ring, then row, then column).
		 * Each job carries its own ring; the pass sets the terrain and object
		 * options from it per job (lodgenchunkpass.cpp). */
		if ( gLgAllRings ) {
			if ( !gLgIncremental.isEmpty() ) {
				err() << "error: --incremental works on one chunk size; --dim all runs four "
						 "(the panel refuses it the same way)" << Qt::endl;
				return 2;
			}
			for ( int rd : { 8, 16, 32 } ) {
				const int rx0 = floorTo( region[0], rd ), ry0 = floorTo( region[1], rd );
				for ( int cy = ry0; cy <= region[3]; cy += rd )
					for ( int cx = rx0; cx <= region[2]; cx += rd )
						jobs.append( LodgenChunkJob{ rd, cx, cy } );
			}
			out() << "rings: 4+8+16+32 (--dim all), " << jobs.size() << " chunk job(s)" << Qt::endl;
		}
"""),
]

PASS = [
(b"""QString chunkKey( int cx, int cy )
{
	return QString( "%1,%2" ).arg( cx ).arg( cy );
}
""", b"""QString chunkKey( int cx, int cy )
{
	return QString( "%1,%2" ).arg( cx ).arg( cy );
}

/* A run over several rings (`--dim all`, lane BAKE1) has a ring-4 and a
 * ring-8 chunk at the same corner. A chunk of the run's OWN ring keeps the
 * old key, so a one-ring record is unchanged to the byte; another ring's
 * chunk is `cx,cy@dim`. Only the record writer and the produced-file list use
 * it: --incremental refuses a many-ring run. */
QString chunkKeyAt( int dim, int runDim, int cx, int cy )
{
	return dim == runDim ? chunkKey( cx, cy )
		: QString( "%1,%2@%3" ).arg( cx ).arg( cy ).arg( dim );
}
"""),
(b"""		rp->producedFiles[chunkKey( r.cx, r.cy )].append( cp );
""", b"""		rp->producedFiles[chunkKeyAt( r.dim, rp->dim, r.cx, r.cy )].append( cp );
"""),
(b"""	QStringList & pf = run.producedFiles[chunkKey( r.cx, r.cy )];
""", b"""	QStringList & pf = run.producedFiles[chunkKeyAt( r.dim, run.dim, r.cx, r.cy )];
"""),
(b"""		oldByKey.insert( chunkKey( e.cx, e.cy ), &e );
	for ( const LodgenChunkJob & j : run.allJobs ) {
		const QString key = chunkKey( j.cx, j.cy );
""", b"""		oldByKey.insert( chunkKeyAt( e.dim, run.dim, e.cx, e.cy ), &e );
	for ( const LodgenChunkJob & j : run.allJobs ) {
		const QString key = chunkKeyAt( j.dim, run.dim, j.cx, j.cy );
"""),
(b"""		e.dim = run.dim; e.cx = j.cx; e.cy = j.cy;
		e.inputs = lodgenChunkInputDigest( world, run.dim, j.cx, j.cy, run.digestRoot );
""", b"""		e.dim = j.dim; e.cx = j.cx; e.cy = j.cy;
		e.inputs = lodgenChunkInputDigest( world, j.dim, j.cx, j.cy, run.digestRoot );
"""),
]

patch('nifcli.cpp', CLI)
patch('lodgenchunkpass.cpp', PASS)
