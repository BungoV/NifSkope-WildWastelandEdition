/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* ---------------------------------------------------------------------------
 * WW_CELL_CENSUS_TEST=<census file> -- lane PRTP5 (docs/PRTP_PLAN.md step 5,
 * "render each cell"), run INSIDE the running application.
 *
 * THE WALK. Every interior of the plugin and every exterior cell of the named
 * worldspace(s), in this one window, through the same `.wwcell` door
 * File > Open uses (a scratch spec file and NifSkope::openFile). The Cell
 * lights row is forced on, the frame is drawn and grabbed, the census rows are
 * appended, the texture cache is flushed, and the next load replaces this one.
 * The whole world is never loaded.
 *
 * ONE LOAD, ONE ROW PER CELL. An interior is a load of its own. The exterior
 * grid is cut into TILES that do not overlap (WW_CELL_CENSUS_BLOCK cells a
 * side, 5; a tile's cells are x in [k*5, k*5+4], so its centre is k*5+2): a
 * tile is loaded ONCE, as the block around its centre cell, and every cell in
 * it gets its own row from that one load. The cell a placed object belongs to
 * comes from the plugin (the reference model keeps it per reference). Before
 * this (v1) each cell was opened as the block around itself, so every cell was
 * loaded 25 times.
 *
 * WHAT A ROW HOLDS. `refs`, `refs_drawn` and `lights_cell` are the CELL's own.
 * Everything from `refs_block` to `rss_mb` is the LOAD's (what the builder
 * said about the tile, what the texture cache was asked for, the frame, the
 * times): it is written on the load's first row and is `^` on the load's
 * other rows, so a column summed over the census is a true total.
 *
 * A VISIT IS A LIST OF STEPS (WW_CELL_CENSUS_STEPS, default `census`). The
 * game is gone through ONCE, and each visit of a load does every step asked
 * for: a step says what the builder must do while the load is built (`before`)
 * and reads what that left behind into the row (`after`). Two steps today:
 *   census  the per-cell check-up: the frame, the builder's counts, what could
 *           not be loaded. Always on -- its rows are also what makes the pass
 *           resumable.
 *   bake    the probe bake as the cell view's headless path does it
 *           (WW_CELL_PROBES + WW_CELL_PROBE_BAKE, set here per load): the
 *           probes of the tile's own cells are placed and baked into
 *           WW_CELL_CENSUS_BAKE/<worldspace or I_<form>>/, and the row carries
 *           the probes, the files written and the bake's milliseconds.
 *
 * THE RING (WW_CELL_CENSUS_MARGIN, default 0). A probe sees only what is
 * loaded in its visit: a ray that meets nothing is sky. So a bake of exterior
 * cells needs their neighbors loaded around them, and the check-up does not.
 * With a ring of M cells a tile of T is opened as the block of T + 2M around
 * its centre; rows and probes are still the tile's own cells only, so cells
 * are loaded more than once but never probed or counted twice.
 *
 * A TILE THAT IS TOO BIG SPLITS. A tile holding more references than
 * WW_CELL_CENSUS_REFS_MAX is opened cell by cell, each alone, and each row
 * says so; so is a tile the walk died on (it is named in `<census>.split`).
 *
 * A TILE WITH NOTHING PLACED IS NOT BUILT, when the check-up is the only step.
 * Its rows are counts (all zero) and need no scene: they are written from the
 * plugin and say `count only`. A bake needs the ground, so with the bake step
 * on every tile is built.
 *
 * RESUMABLE. Rows are APPENDED; a cell whose key is already in the file is
 * skipped. The keys of the load in progress sit in `<census>.pending`, so a
 * walk that died names what it died on: the next run writes a lone cell a
 * `crashed` row, and marks a tile to be opened cell by cell.
 *
 * SEVERAL WALKERS. WW_CELL_CENSUS_SLICE=i/N: the walk's units (an interior, a
 * tile) are numbered in plan order and unit u belongs to slice (u mod N) + 1.
 * Walker i walks only its slice and writes its own part file
 * (`<census>.part<i>of<N>.tsv`, with its own pending, split, spec and log
 * files), so N windows can run at once; tests/spells/cell_census_merge.py
 * joins the parts.
 *
 *   WW_CELL_CENSUS_PLUGINS   the load order, as a `.wwcell` names it (required)
 *   WW_CELL_CENSUS_WORLD     worldspace EDIDs, comma separated; `all`; `none`
 *                            (default Commonwealth)
 *   WW_CELL_CENSUS_NOINTERIORS=1   exteriors only
 *   WW_CELL_CENSUS_BLOCK     cells a side of an exterior tile, odd (default 5)
 *   WW_CELL_CENSUS_SLICE     i/N: walk slice i of N (default 1/1)
 *   WW_CELL_CENSUS_STEPS     what a visit does: `census` or `census,bake`
 *   WW_CELL_CENSUS_BAKE      the bake's folder (required by the bake step)
 *   WW_CELL_CENSUS_TBK       the `.tbk` version the bake step asks for: 4
 *                            (default) or 3. The builder is handed it as
 *                            WW_CELL_PROBE_BAKE_TBK, and every file a visit
 *                            wrote is read back: another version fails the
 *                            visit. On 2026-10-02 the cell view's bake does not
 *                            read that variable yet and writes its default (4),
 *                            so a run that asks for 3 FAILS here, on purpose,
 *                            instead of handing a v3 reader v4 files
 *   WW_CELL_CENSUS_MARGIN    cells of ring loaded around an exterior tile (0)
 *   WW_CELL_CENSUS_PLAN      write the whole walk plan here (key, form, EDID,
 *                            unit, place among the units walked, slice)
 *   WW_CELL_CENSUS_ONLY      a file of keys: walk only the units these are in
 *   WW_CELL_CENSUS_BUDGET    seconds after which no new load is started (480)
 *   WW_CELL_CENSUS_MAX       at most this many units this run
 *   WW_CELL_CENSUS_RSS_MAX   MB of memory held after which no new load is
 *                            started (8000; 0 = no ceiling)
 *   WW_CELL_CENSUS_REFS_MAX  a tile holding more references than this is
 *                            opened cell by cell (12000; 0 = never split)
 *   WW_CELL_CENSUS_SETTLE_MS wait between the load and the frame (250)
 *   WW_CELL_CENSUS_SHOTS     a folder: `<key>.png` and `<key>.notes` per load
 *   WW_CELL_CENSUS_RED       stale | dropcell | dropslice | doubleslice |
 *                            nobake -- the gate's red controls
 *   WW_CELL_DATAROOT         as for any cell (the builder reads it)
 *
 * WW_CELL_OPEN must NOT be set: it overrides every spec file, so every load
 * of the walk would be the same cell. The harness refuses to start if it is.
 *
 * A harness FORCES the state it measures: the Cell lights row is switched on
 * here for every load, and a load whose lights were not on fails its check.
 * --------------------------------------------------------------------------- */

#include "cellrefs.h"
#include "cellview.h"
#include "cellworkspace.h"
#include "esmdata.h"
#include "glview.h"
#include "nifskope.h"
#include "gl/celllights.h"
#include "gl/glscene.h"
#include "gl/gltex.h"
#include "model/nifmodel.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUndoStack>

#include <map>
#include <memory>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {

//! One cell of the walk.
struct CensusCell
{
	QString key;            //!< `I:<form>` or `E:<world>:<x>,<y>`
	bool interior = true;
	QString world;
	int x = 0, y = 0;
	quint32 form = 0;
	QString edid;
	int unit = -1;          //!< the unit (interior, or exterior tile) the cell is in
};

//! One unit of the walk: an interior, or one exterior tile. A unit is what a slice owns.
struct CensusUnit
{
	bool interior = true;
	QString world;
	int tx = 0, ty = 0;     //!< an exterior tile's centre cell
	QVector<int> cells;     //!< plan indices, plan order
	bool wanted = true;     //!< false when a sample file names none of its cells
	int order = -1;         //!< its place among the wanted units
	int slice = 0;          //!< the slice that owns it, 1..N
};

//! One load of the window: an interior, a whole tile, or one cell of a split tile.
struct CensusLoad
{
	int unit = -1;
	QVector<int> cells;     //!< plan indices this load writes rows for
	int block = 1;          //!< what it is opened as (the cells it is for, plus the ring)
	int own = 1;            //!< exterior: the middle `own` x `own` cells are the load's own
	int cx = 0, cy = 0;     //!< exterior: the centre cell of what is opened
	qint64 refs = -1;       //!< exterior: the walk's own count of what the load reads
	QString note;           //!< why a cell of a split tile is opened alone
};

//! What was counted for one row. -1 = the builder did not say.
struct CensusRow
{
	QString status = QStringLiteral( "ok" );
	int block = 1;                  //!< what the load was opened as
	// ---- the cell's own
	qint64 refs = -1, refsDrawn = -1, lightsCell = -1;
	// ---- the load's
	qint64 refsBlock = -1, placements = -1;
	qint64 shapes = -1, verts = -1, tris = -1;
	qint64 lightsBlock = -1, lit = -1, omni = -1, spot = -1;
	qint64 skipOff = -1, skipNoRadius = -1, skipBlack = -1, ambientOnly = -1;
	QString lightTypes;
	qint64 lightsApprox = -1;       //!< hemisphere and box lights: read, drawn as plain omnis
	qint64 modelsLoaded = -1, modelsFailed = -1;
	int texAsked = -1, texMissing = -1;
	qint64 matsUnreadable = -1;
	qint64 coverPx = -1;
	QString fb;
	QString steps;                  //!< the steps this visit did, `+` joined
	qint64 bakeProbes = -1, bakeFiles = -1;
	qint64 buildMs = -1;            //!< the whole load, the bake inside it
	qint64 bakeMs = -1;             //!< of buildMs: placing the probes, the rays, the files
	qint64 renderMs = -1, totalMs = -1;
	double rssMb = -1.0;
	QString note;
};

/*! ONE STEP OF A VISIT. `before` runs before the load is opened and says what
 *  the builder must do while it builds; `after` runs once the load is built
 *  and reads what the step left behind into the row. A new product of the
 *  whole-game pass is a new entry in kSteps, not a second pass. */
struct VisitStep
{
	const char * name;
	bool needsScene;        //!< false: rows about nothing placed need no load for it
	void ( *before )( const CensusLoad & ld );
	void ( *after )( const CensusLoad & ld, CensusRow & r );
};

struct Walk
{
	NifSkope * skope = nullptr;
	QString censusPath, planPath, shotDir, logPath, specPath, pendingPath, splitPath;
	QString plugins, red;
	int block = 5, settleMs = 250, budgetS = 480, maxUnits = -1, rssMaxMb = 8000;
	int margin = 0;             //!< cells of ring loaded around an exterior tile
	int sliceI = 1, sliceN = 1;
	QVector<const VisitStep *> steps;   //!< what a visit does, in order
	QString stepNames;          //!< `census` or `census+bake`
	bool needScene = false;     //!< some step needs the scene even where nothing is placed
	QString bakeDir;            //!< the bake step's folder
	int tbk = 4;                //!< the `.tbk` version the bake step asks for (WW_CELL_CENSUS_TBK)
	qint64 loadStartMs = 0;     //!< wall clock when the load was opened (the bake step's files are newer)
	QVector<CensusCell> plan;   //!< every cell of the walk, plugin order
	QVector<CensusUnit> units;  //!< every unit, in the order its first cell is planned
	QVector<int> todo;          //!< unit indices still to do this run
	QVector<CensusLoad> queue;  //!< the loads of the unit being walked
	CensusLoad load;            //!< the load that is open
	QSet<QString> have;         //!< keys with a row in this census file
	QSet<QString> splitTiles;   //!< tiles the walk died on: opened cell by cell
	int planInteriors = 0, planExteriors = 0;
	int onlyNamed = 0, onlyUnknown = 0;
	int unitsWanted = 0, unitsMine = 0;
	int already = 0;
	bool waiting = false, finished = false;
	int done = 0, loads = 0, ok = 0, refused = 0, rows = 0, countOnly = 0, crashedRows = 0;
	int splitUnits = 0, diedTiles = 0;
	int checks = 0, failures = 0;
	int versionRead = 0;        //!< visits whose bake files were read back as the version asked for
	QStringList failLines;
	QElapsedTimer wall, cell;
	qint64 buildMs = 0;
	qint64 msInterior = 0, msExterior = 0;
	int nInterior = 0, nExterior = 0, nExteriorCells = 0;
	QString notes, error;       //!< the builder's own words for the load being opened
	qint64 prevRefs = -1, prevDrawn = -1, prevLights = -1;  //!< the last cell's own counts (the stale red)
	bool havePrev = false;
	/* A TILE THAT IS TOO BIG SPLITS. The window's memory grows with the
	 * references it reads and a downtown tile holds nine times what the
	 * Sanctuary one does, so a tile over refsMax is opened cell by cell. */
	int refsMax = 12000;
	std::map<QString, std::unique_ptr<EsmWorld>> worlds;
};

Walk g;
QtMessageHandler g_prevHandler = nullptr;

const char * const kColumns =
	"key\tkind\tworld\tx\ty\tform\tedid\tblock\ttile\tslice\tstatus\trefs\trefs_drawn\trefs_block\tplacements"
	"\tshapes\tverts\ttris\tlights_cell\tlights_block\tlit\tomni\tspot\tskip_off\tskip_noradius\tskip_black"
	"\tambient_only\tlight_types\tlights_approx\tmodels_loaded\tmodels_failed\ttex_asked\ttex_missing"
	"\tmats_unreadable\tfar\tcover_px\tfb\tsteps\tbake_probes\tbake_files\tbuild_ms\tbake_ms\trender_ms\ttotal_ms"
	"\trss_mb\tnote";

QString envStr( const char * name, const QString & def = QString() )
{
	const QByteArray v = qgetenv( name );
	return v.isEmpty() ? def : QString::fromLocal8Bit( v );
}

int envInt( const char * name, int def )
{
	bool ok = false;
	const int v = envStr( name ).trimmed().toInt( &ok );
	return ok ? v : def;
}

QString hex8( quint32 v )
{
	return QString::number( v, 16 ).rightJustified( 8, QLatin1Char( '0' ) ).toUpper();
}

//! Floor of a / b, for a positive b (C++ division rounds toward zero).
int floorDiv( int a, int b )
{
	int q = a / b;
	if ( ( a % b ) != 0 && a < 0 )
		q--;
	return q;
}

//! The centre cell of the tile this cell is in, along one axis.
int tileCentre( int v, int block )
{
	return floorDiv( v, block ) * block + block / 2;
}

QString tileId( const QString & world, int tx, int ty )
{
	return QStringLiteral( "%1:%2,%3" ).arg( world ).arg( tx ).arg( ty );
}

//! One field of a row: no tabs, no line breaks.
QString flat( QString s )
{
	s.replace( QLatin1Char( '\t' ), QLatin1Char( ' ' ) );
	s.replace( QLatin1Char( '\r' ), QLatin1Char( ' ' ) );
	s.replace( QLatin1Char( '\n' ), QLatin1String( " / " ) );
	/* plain ASCII only: a base whose "model" is not a name reaches the notes as
	 * raw bytes, and the census is a text file */
	QString out;
	out.reserve( s.size() );
	for ( const QChar ch : s ) {
		const ushort u = ch.unicode();
		if ( u >= 0x20 && u < 0x7f )
			out += ch;
		else
			out += QStringLiteral( "\\x%1" ).arg( u, 2, 16, QLatin1Char( '0' ) );
	}
	return out.trimmed();
}

double rssMb()
{
#ifdef Q_OS_WIN
	PROCESS_MEMORY_COUNTERS pmc;
	if ( K32GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) ) )
		return double( pmc.WorkingSetSize ) / 1048576.0;
#endif
	return -1.0;
}

/* THE BUILDER'S OWN WORDS. The `.wwcell` door prints the census as
 * `qInfo() << "cell view:\n" << notes` and a refusal as
 * `qWarning() << "cell view:" << error`; both are caught here, on the GUI
 * thread, for the load that is being opened, and everything else is passed on. */
void captureMessages( QtMsgType type, const QMessageLogContext & ctx, const QString & str )
{
	if ( g.waiting && str.startsWith( QLatin1String( "cell view:" ) )
		&& QThread::currentThread() == qApp->thread() ) {
		QString body = str.mid( 10 ).trimmed();
		if ( type == QtInfoMsg ) {
			g.notes = body;
			return;     // forty lines a load would bury the run's log; the census is the record
		}
		if ( body.size() >= 2 && body.startsWith( QLatin1Char( '"' ) ) && body.endsWith( QLatin1Char( '"' ) ) )
			body = body.mid( 1, body.size() - 2 );
		body.replace( QLatin1String( "\\\"" ), QLatin1String( "\"" ) );
		g.error = body;
	}
	if ( g_prevHandler )
		g_prevHandler( type, ctx, str );
}

void check( bool ok, const QString & what, const QString & detail = QString() )
{
	g.checks++;
	if ( ok )
		return;
	g.failures++;
	if ( g.failLines.size() < 200 )
		g.failLines.append( detail.isEmpty() ? what : QStringLiteral( "%1   [%2]" ).arg( what, detail ) );
}

qint64 noteNumber( const char * pattern )
{
	const QRegularExpressionMatch m = QRegularExpression( QLatin1String( pattern ) ).match( g.notes );
	return m.hasMatch() ? m.captured( 1 ).toLongLong() : -1;
}

QString noteText( const char * pattern )
{
	const QRegularExpressionMatch m = QRegularExpression( QLatin1String( pattern ) ).match( g.notes );
	return m.hasMatch() ? m.captured( 1 ) : QString();
}

//! Append whole lines in one write, so the rows of a load land together or not at all.
void appendLines( const QString & path, const QStringList & lines )
{
	QFile f( path );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream s( &f );
		s << lines.join( QLatin1Char( '\n' ) ) << "\n";
	}
}

/*! One census row. `lead` is the load's first row: it carries the load's
 *  figures; the load's other rows carry `^` there. */
QString rowLine( const CensusCell & c, const CensusRow & r, bool lead )
{
	QStringList f;
	auto n = [&f]( qint64 v ) { f.append( v < 0 ? QStringLiteral( "-" ) : QString::number( v ) ); };
	const QString same = QStringLiteral( "^" );
	const CensusUnit * u = ( c.unit >= 0 && c.unit < g.units.size() ) ? &g.units.at( c.unit ) : nullptr;
	f << c.key << ( c.interior ? QStringLiteral( "interior" ) : QStringLiteral( "exterior" ) )
	  << ( c.interior ? QStringLiteral( "-" ) : c.world )
	  << ( c.interior ? QStringLiteral( "-" ) : QString::number( c.x ) )
	  << ( c.interior ? QStringLiteral( "-" ) : QString::number( c.y ) )
	  << hex8( c.form ) << ( c.edid.isEmpty() ? QStringLiteral( "-" ) : flat( c.edid ) )
	  << QString::number( c.interior ? 1 : r.block )
	  << ( ( c.interior || !u ) ? QStringLiteral( "-" ) : QStringLiteral( "%1,%2" ).arg( u->tx ).arg( u->ty ) )
	  << QStringLiteral( "%1/%2" ).arg( g.sliceI ).arg( g.sliceN )
	  << r.status;
	n( r.refs ); n( r.refsDrawn );
	if ( !lead ) {
		// refs_block placements shapes verts tris
		for ( int i = 0; i < 5; i++ )
			f << same;
		n( r.lightsCell );
		// lights_block .. mats_unreadable (15), far, cover_px .. rss_mb (10)
		for ( int i = 0; i < 15; i++ )
			f << same;
		f << ( c.interior ? QStringLiteral( "-" ) : QStringLiteral( "none" ) );
		for ( int i = 0; i < 10; i++ )
			f << same;
		f << ( r.note.isEmpty() ? QStringLiteral( "-" ) : flat( r.note ) );
		return f.join( QLatin1Char( '\t' ) );
	}
	n( r.refsBlock ); n( r.placements );
	n( r.shapes ); n( r.verts ); n( r.tris );
	n( r.lightsCell ); n( r.lightsBlock ); n( r.lit ); n( r.omni ); n( r.spot );
	n( r.skipOff ); n( r.skipNoRadius ); n( r.skipBlack ); n( r.ambientOnly );
	f << ( r.lightTypes.isEmpty() ? QStringLiteral( "-" ) : flat( r.lightTypes ) );
	n( r.lightsApprox ); n( r.modelsLoaded ); n( r.modelsFailed ); n( r.texAsked ); n( r.texMissing );
	n( r.matsUnreadable );
	/* THE FAR FIELD. The ruling is "LOD beyond the block"; the cell view draws
	 * the block and nothing beyond it, so the column says so on every row
	 * instead of leaving a reader to assume the far field was in the frame. */
	f << ( c.interior ? QStringLiteral( "-" ) : QStringLiteral( "none" ) );
	n( r.coverPx );
	f << ( r.fb.isEmpty() ? QStringLiteral( "-" ) : r.fb );
	f << ( r.steps.isEmpty() ? QStringLiteral( "-" ) : r.steps );
	n( r.bakeProbes ); n( r.bakeFiles );
	n( r.buildMs ); n( r.bakeMs ); n( r.renderMs ); n( r.totalMs );
	f << ( r.rssMb < 0.0 ? QStringLiteral( "-" ) : QString::number( r.rssMb, 'f', 0 ) );
	f << ( r.note.isEmpty() ? QStringLiteral( "-" ) : flat( r.note ) );
	return f.join( QLatin1Char( '\t' ) );
}

void closeMessageBoxes()
{
	// a failed load leaves a non-modal box behind; a walk would collect thousands
	for ( QMessageBox * b : g.skope->findChildren<QMessageBox *>() )
		b->close();
}

void markClean()
{
	// a generated document is never a modified one: nothing may ask to save it
	if ( NifModel * nif = g.skope->getNifModel() ) {
		if ( nif->undoStack ) {
			nif->undoStack->clear();
			nif->undoStack->setClean();
		}
	}
	g.skope->setWindowModified( false );
}

void flushTextures()
{
	// one load's textures at a time: the cache is not emptied by a new document
	if ( GLView * ogl = g.skope->getGLView() ) {
		ogl->makeCurrent();
		ogl->flush();
		ogl->doneCurrent();
	}
}

void finishWalk( const QString & why );
void openNext();

/*! How many references opening this block reads, counted the way the builder
 *  reads them: every cell's own, then the worldspace's persistent references
 *  that stand inside the block and were not among those. */
qint64 blockRefs( const EsmWorld & w, int cx, int cy, int block )
{
	const int h = block / 2;
	QSet<quint32> seen;
	qint64 n = 0;
	for ( int y = cy - h; y <= cy + h; y++ ) {
		for ( int x = cx - h; x <= cx + h; x++ ) {
			if ( !w.hasCell( x, y ) )
				continue;
			for ( const EsmRefr & r : w.refrs( x, y ) ) {
				seen.insert( r.formID );
				n++;
			}
		}
	}
	const float u = 4096.0f;
	for ( const EsmRefr & r : w.persistentRefrsIn( float( cx - h ) * u, float( cy - h ) * u,
			float( cx + h + 1 ) * u, float( cy + h + 1 ) * u ) ) {
		if ( !seen.contains( r.formID ) )
			n++;
	}
	return n;
}

//! The placed lights of what is loaded (deleted ones left out), and how many stand in each grid square.
qint64 placedLights( QHash<QPair<int, int>, qint64> & byGrid )
{
	const CellRefTable & table = cellRefTable();
	const quint32 kLigh = quint32( 'L' ) | ( quint32( 'I' ) << 8 ) | ( quint32( 'G' ) << 16 ) | ( quint32( 'H' ) << 24 );
	qint64 n = 0;
	for ( int i = 0; i < table.size(); i++ ) {
		const CellRefEntry & e = table.at( i );
		if ( e.baseType != kLigh || e.fate == CellRefFate::Deleted )
			continue;
		n++;
		byGrid[qMakePair( e.cellX, e.cellY )]++;
	}
	return n;
}

//! The load is over (rows written or not): the next one, from the event loop.
void closeLoad( qint64 totalMs )
{
	QFile::remove( g.pendingPath );
	g.loads++;
	const bool interior = g.units.at( g.load.unit ).interior;
	( interior ? g.msInterior : g.msExterior ) += totalMs;
	( interior ? g.nInterior : g.nExterior )++;
	if ( !interior )
		g.nExteriorCells += g.load.cells.size();
	if ( g.queue.isEmpty() )
		g.done++;
	QTimer::singleShot( 0, g.skope, []() { openNext(); } );
}

QString loadFileBase( const CensusLoad & ld )
{
	QString base = g.plan.at( ld.cells.first() ).key;
	base.replace( QLatin1Char( ':' ), QLatin1Char( '_' ) );
	base.replace( QLatin1Char( ',' ), QLatin1Char( '_' ) );
	return base;
}

/*! Where the bake step puts a load's files: sector files are named by position,
 *  and every interior has its own origin, so an interior gets its own folder
 *  and a worldspace one for all its tiles. */
QString bakeFolder( const CensusLoad & ld )
{
	const CensusUnit & u = g.units.at( ld.unit );
	return g.bakeDir + QLatin1Char( '/' )
		+ ( u.interior ? QStringLiteral( "I_%1" ).arg( hex8( g.plan.at( ld.cells.first() ).form ) ) : u.world );
}

/* ---- STEP `bake`: the probe bake, by the cell view's own headless path. The
 * builder reads these three variables every time it builds, so setting them
 * here per load is all the hook there is; nothing of the bake is done here. */
void stepBakeBefore( const CensusLoad & ld )
{
	if ( g.red == QLatin1String( "nobake" ) )
		return;     // RED `nobake` (the gate's control): the row says bake, the builder was never told
	const QString dir = bakeFolder( ld );
	QDir().mkpath( dir );
	qputenv( "WW_CELL_PROBES", QDir::toNativeSeparators( dir + QStringLiteral( "/probes_%1.tsv" ).arg( loadFileBase( ld ) ) ).toLocal8Bit() );
	qputenv( "WW_CELL_PROBE_BAKE", QDir::toNativeSeparators( dir ).toLocal8Bit() );
	qputenv( "WW_CELL_PROBES_N", QByteArray::number( ld.own ) );     // the load's own cells, never the ring
	qputenv( "WW_CELL_PROBES_HIDE", "1" );      // the markers are not part of the cell: the frame stays the check-up's
	// the file version is the run's choice (WW_CELL_CENSUS_TBK, default 4). The builder is told here; whether it
	// listened is read from the files in stepBakeAfter, so a run never says one version and writes another.
	qputenv( "WW_CELL_PROBE_BAKE_TBK", QByteArray::number( g.tbk ) );
}

//! The version a `.tbk` sector file says it is (its second 32-bit word), or -1.
int tbkVersionOf( const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return -1;
	const QByteArray h = f.read( 8 );
	if ( h.size() < 8 || h.left( 4 ) != QByteArray( "TBK1" ) )
		return -1;
	return int( quint8( h[4] ) ) | ( int( quint8( h[5] ) ) << 8 ) | ( int( quint8( h[6] ) ) << 16 ) | ( int( quint8( h[7] ) ) << 24 );
}

void stepBakeAfter( const CensusLoad & ld, CensusRow & r )
{
	qunsetenv( "WW_CELL_PROBES" );
	qunsetenv( "WW_CELL_PROBE_BAKE" );
	qunsetenv( "WW_CELL_PROBES_N" );
	qunsetenv( "WW_CELL_PROBES_HIDE" );
	qunsetenv( "WW_CELL_PROBE_BAKE_TBK" );
	const QString leadKey = g.plan.at( ld.cells.first() ).key;
	r.bakeProbes = noteNumber( "bake: (\\d+) probes" );
	// what the bake cost inside the build: placing the probes, the rays, the files
	qint64 ms = 0;
	bool timed = false;
	for ( const char * pat : { "probe time ms: bvh (\\d+)", "probe time ms: [^\\n]*columns (\\d+)", "probe time ms: [^\\n]*voxels (\\d+)",
			"probe time ms: [^\\n]*openings (\\d+)", "probe time ms: [^\\n]*rooms (\\d+)",
			"bake time ms: rays (\\d+)", "bake time ms: rays \\d+, write (\\d+)" } ) {
		const qint64 v = noteNumber( pat );
		if ( v >= 0 ) {
			ms += v;
			timed = true;
		}
	}
	r.bakeMs = timed ? ms : -1;
	// the files: the sector files in the load's folder that this load wrote
	const QString dir = bakeFolder( ld );
	qint64 files = 0, otherVersion = 0;
	int seenVersion = -1;
	for ( const QFileInfo & fi : QDir( dir ).entryInfoList( { QStringLiteral( "sector_*.tbk" ) }, QDir::Files ) )
		if ( fi.size() > 0 && fi.lastModified().toMSecsSinceEpoch() >= g.loadStartMs - 2000 ) {
			files++;
			const int v = tbkVersionOf( fi.absoluteFilePath() );
			if ( v != g.tbk ) {
				otherVersion++;
				seenVersion = v;
			}
		}
	r.bakeFiles = files;
	if ( files > 0 )
		check( otherVersion == 0, QStringLiteral( "%1: the bake's files are the version the run asked for (.tbk v%2)" ).arg( leadKey ).arg( g.tbk ),
			QStringLiteral( "%1 of %2 files say another version (v%3): the builder did not take WW_CELL_PROBE_BAKE_TBK" )
				.arg( otherVersion ).arg( files ).arg( seenVersion ) );
	if ( files > 0 && otherVersion == 0 )
		g.versionRead++;
	const qint64 sectors = noteNumber( "unlinked mean [0-9.]+, sectors (\\d+)" );
	const bool named = g.notes.contains( QLatin1String( "bake folder " ) );
	check( named && r.bakeProbes > 0 && files > 0 && files == sectors,
		QStringLiteral( "%1: the bake step wrote its files in this visit" ).arg( leadKey ),
		QStringLiteral( "notes name the folder: %1; probes %2; sector files written now %3, the notes say %4; folder %5" )
			.arg( named ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ).arg( r.bakeProbes ).arg( files ).arg( sectors ).arg( dir ) );
}

/* ---- STEP `census`: the per-cell check-up. The frame, the builder's own
 * counts, what the texture cache could not load, and the checks a visit
 * stands on. */
void stepCensusAfter( const CensusLoad & ld, CensusRow & r )
{
	const CensusUnit & unit = g.units.at( ld.unit );
	const QString leadKey = g.plan.at( ld.cells.first() ).key;
	NifSkope * skope = g.skope;
	GLView * ogl = skope->getGLView();
	NifModel * nif = skope->getNifModel();
	Scene * scene = ogl ? ogl->getScene() : nullptr;

	// THE STATE THIS MEASURES, FORCED: the Cell lights row, on
	if ( QCheckBox * row = skope->findChild<QCheckBox *>( QStringLiteral( "CellWorkspaceCellLights" ) ) ) {
		if ( !row->isChecked() )
			row->setChecked( true );
	}
	wwCellLightsSetOn( true );

	// ---- the frame
	QElapsedTimer rt;
	rt.start();
	QImage fb;
	if ( ogl ) {
		/* setOrientation returns early when the view is already the one asked
		 * for, so from the second load on it would not refit: center() asks
		 * for the fit by itself, and the fit runs inside the next paint. */
		ogl->setOrientation( GLView::ViewTop, true );
		ogl->center();
		for ( int i = 0; i < 3; i++ ) {
			ogl->update();
			qApp->processEvents();
		}
		fb = ogl->grabFramebuffer();
	}
	r.renderMs = rt.elapsed();
	if ( !fb.isNull() ) {
		const Color4 bg = ogl->clearColor();
		const int br = int( bg.red() * 255.0f + 0.5f ), bgc = int( bg.green() * 255.0f + 0.5f ),
			bb = int( bg.blue() * 255.0f + 0.5f );
		const QImage img = fb.convertToFormat( QImage::Format_RGB32 );
		qint64 cover = 0;
		for ( int y = 0; y < img.height(); y++ ) {
			const QRgb * p = reinterpret_cast<const QRgb *>( img.constScanLine( y ) );
			for ( int x = 0; x < img.width(); x++ )
				if ( qAbs( qRed( p[x] ) - br ) > 2 || qAbs( qGreen( p[x] ) - bgc ) > 2 || qAbs( qBlue( p[x] ) - bb ) > 2 )
					cover++;
		}
		r.coverPx = cover;
		r.fb = QStringLiteral( "%1x%2" ).arg( img.width() ).arg( img.height() );
	}

	// ---- the builder's notes: the load's figures
	r.refsBlock = noteNumber( "refrs read (\\d+)" );
	r.placements = noteNumber( "placements (\\d+)" );
	r.shapes = noteNumber( "welded shapes (\\d+)" );
	r.verts = noteNumber( "welded shapes \\d+, vertices (\\d+)" );
	r.tris = noteNumber( "vertices \\d+, triangles (\\d+)" );
	r.lightsBlock = noteNumber( "lights: (\\d+) placed" );
	r.lightTypes = noteText( "lights: \\d+ placed \\(([^)]*)\\)" );
	r.lit = noteNumber( "cell lighting: lights=(\\d+)" );
	r.omni = noteNumber( "cell lighting: lights=\\d+ \\(omni (\\d+)" );
	r.spot = noteNumber( "cell lighting: lights=\\d+ \\(omni \\d+, spot (\\d+)" );
	r.skipOff = noteNumber( "skipped: off (\\d+)" );
	r.skipNoRadius = noteNumber( "skipped: off \\d+, no radius (\\d+)" );
	r.skipBlack = noteNumber( "no radius \\d+, black (\\d+)" );
	r.ambientOnly = noteNumber( " ambientonly=(\\d+)" );
	r.modelsLoaded = noteNumber( "distinct models loaded (\\d+)" );
	r.modelsFailed = noteNumber( "failed to load (\\d+)" );
	r.matsUnreadable = noteNumber( "(\\d+) drawn neutral grey" );
	{
		// hemisphere and box lights are read and counted, and drawn as plain omnis
		qint64 approx = 0;
		const QRegularExpression re( QStringLiteral( "(shadow hemi|box) (\\d+)" ) );
		QRegularExpressionMatchIterator it = re.globalMatch( r.lightTypes );
		while ( it.hasNext() )
			approx += it.next().captured( 2 ).toLongLong();
		r.lightsApprox = r.lightsBlock < 0 ? -1 : approx;
	}

	// ---- the reference model: the load's placed lights
	QHash<QPair<int, int>, qint64> lightsByGrid;
	const qint64 lightsTable = placedLights( lightsByGrid );

	// ---- the texture cache: what this load's frames asked for
	QStringList texNames;
	if ( scene && scene->textures )
		scene->textures->wwAskedAndMissing( r.texAsked, r.texMissing, &texNames );

	// ---- what could not be loaded, in words
	QStringList note;
	const QString failedNames = noteText( "models failed: ([^\\n]*)" );
	if ( r.modelsFailed > 0 )
		note.append( QStringLiteral( "models failed: %1" ).arg( failedNames.isEmpty() ? QString::number( r.modelsFailed ) : failedNames ) );
	if ( r.texMissing > 0 ) {
		std::sort( texNames.begin(), texNames.end() );
		note.append( QStringLiteral( "textures missing: %1%2" ).arg( texNames.mid( 0, 6 ).join( QLatin1String( ", " ) ),
			texNames.size() > 6 ? QStringLiteral( ", ..." ) : QString() ) );
	}
	if ( r.matsUnreadable > 0 )
		note.append( QStringLiteral( "materials unreadable: %1" ).arg( r.matsUnreadable ) );
	if ( r.lightsApprox > 0 )
		note.append( QStringLiteral( "lights drawn as omni: %1" ).arg( r.lightsApprox ) );
	if ( !ld.note.isEmpty() )
		note.append( ld.note );
	for ( const QString & line : g.notes.split( QLatin1Char( '\n' ) ) )
		if ( line.contains( QLatin1String( "REFUSED" ) ) )
			note.append( line.trimmed() );
	r.note = note.join( QLatin1String( "; " ) );

	// ---- the checks this load stands on
	const QString head = g.notes.section( QLatin1Char( '\n' ), 0, 0 );
	const QString wantHead = unit.interior
		? QStringLiteral( "form 0x%1 " ).arg( hex8( g.plan.at( ld.cells.first() ).form ).toLower() )
		: QStringLiteral( "cell view %1 %2,%3 block %4x%4 " ).arg( unit.world ).arg( ld.cx ).arg( ld.cy ).arg( ld.block );
	check( head.contains( wantHead ), QStringLiteral( "%1: the builder's notes are for the load asked for" ).arg( leadKey ), head );
	check( wwCellLightsOn() && scene && wwCellLightsWanted( scene ),
		QStringLiteral( "%1: the Cell lights row is on for the frame" ).arg( leadKey ) );
	{
		const WwCellLighting * L = nif ? wwCellLightsFor( nif ) : nullptr;
		check( L && qint64( L->lights.size() ) == r.lit,
			QStringLiteral( "%1: the lights handed to the shader are the ones the notes count" ).arg( leadKey ),
			QStringLiteral( "%1 vs %2" ).arg( L ? L->lights.size() : -1 ).arg( r.lit ) );
	}
	check( lightsTable == r.lightsBlock,
		QStringLiteral( "%1: the reference model's placed lights equal the builder's lights line" ).arg( leadKey ),
		QStringLiteral( "%1 vs %2" ).arg( lightsTable ).arg( r.lightsBlock ) );
	check( !fb.isNull() && ( r.shapes <= 0 || r.coverPx > 0 ),
		QStringLiteral( "%1: a frame came back, and it is not empty when shapes were drawn" ).arg( leadKey ),
		QStringLiteral( "%1 cover %2 shapes %3" ).arg( r.fb ).arg( r.coverPx ).arg( r.shapes ) );
	if ( !unit.interior )
		check( ld.refs == r.refsBlock,
			QStringLiteral( "%1: the builder read as many references as the walk counted before it opened the block" ).arg( leadKey ),
			QStringLiteral( "walk %1, builder %2" ).arg( ld.refs ).arg( r.refsBlock ) );

	if ( !g.shotDir.isEmpty() ) {
		const QString base = loadFileBase( ld );
		if ( !fb.isNull() )
			fb.save( g.shotDir + QLatin1Char( '/' ) + base + QStringLiteral( ".png" ) );
		QFile nf( g.shotDir + QLatin1Char( '/' ) + base + QStringLiteral( ".notes" ) );
		if ( nf.open( QIODevice::WriteOnly | QIODevice::Text ) )
			nf.write( g.notes.toUtf8() );
	}
}

/* THE STEPS A VISIT CAN DO, in the order a visit does them. */
const VisitStep kSteps[] = {
	{ "census", false, nullptr, stepCensusAfter },
	{ "bake", true, stepBakeBefore, stepBakeAfter },
};

//! The load is built and its first frames are queued: every step reads it, then its rows are written.
void measureLoad()
{
	const CensusLoad & ld = g.load;
	CensusRow r;
	r.buildMs = g.buildMs;
	r.block = ld.block;
	r.steps = g.stepNames;
	for ( const VisitStep * st : g.steps )
		if ( st->after )
			st->after( ld, r );

	flushTextures();
	r.totalMs = g.cell.elapsed();
	r.rssMb = rssMb();
	g.ok++;

	// ---- one row per cell of the load: the cell's own counts, from the reference model
	const CellRefTable & table = cellRefTable();
	QHash<QPair<int, int>, qint64> lightsByGrid;
	const qint64 lightsTable = placedLights( lightsByGrid );
	QStringList lines;
	bool lead = true;
	for ( int pi : ld.cells ) {
		const CensusCell & c = g.plan.at( pi );
		int at = -1;
		for ( int i = 0; i < table.cellCount(); i++ ) {
			const CellBlockEntry & e = table.cellAt( i );
			if ( c.interior ? ( e.cellForm == c.form ) : ( e.cx == c.x && e.cy == c.y ) )
				at = i;
		}
		check( at >= 0 && table.cellAt( at ).cellForm == c.form,
			QStringLiteral( "%1: the reference model holds this cell, by form" ).arg( c.key ),
			at >= 0 ? hex8( table.cellAt( at ).cellForm ) : QStringLiteral( "no such cell in the model" ) );
		CensusRow cr = r;
		if ( !lead )
			cr.note.clear();
		if ( at >= 0 ) {
			cr.refs = table.cellAt( at ).references;
			cr.refsDrawn = table.cellAt( at ).drawn;
			cr.lightsCell = c.interior ? lightsTable : lightsByGrid.value( qMakePair( c.x, c.y ), 0 );
		}
		const qint64 trueRefs = cr.refs, trueDrawn = cr.refsDrawn, trueLights = cr.lightsCell;
		/* RED `stale` (the gate's control): the row carries the counts of the
		 * cell BEFORE it -- what a one-window walk writes if it reads the window
		 * before the new load has replaced the old one. Only an outside reader
		 * of the plugin can tell. */
		if ( g.red == QLatin1String( "stale" ) && g.havePrev ) {
			cr.refs = g.prevRefs;
			cr.refsDrawn = g.prevDrawn;
			cr.lightsCell = g.prevLights;
		}
		g.prevRefs = trueRefs;
		g.prevDrawn = trueDrawn;
		g.prevLights = trueLights;
		g.havePrev = true;
		lines.append( rowLine( c, cr, lead ) );
		g.have.insert( c.key );
		g.rows++;
		lead = false;
	}
	appendLines( g.censusPath, lines );
	closeLoad( r.totalMs );
}

//! The builder refused the load: every row of it carries the reason.
void refuseLoad( const QString & why )
{
	const CensusLoad & ld = g.load;
	CensusRow r;
	r.status = QStringLiteral( "refused" );
	r.block = ld.block;
	r.steps = g.stepNames;
	// a refused load built nothing: what the steps asked of the builder must not follow into the next
	for ( const char * v : { "WW_CELL_PROBES", "WW_CELL_PROBE_BAKE", "WW_CELL_PROBES_N", "WW_CELL_PROBES_HIDE", "WW_CELL_PROBE_BAKE_TBK" } )
		qunsetenv( v );
	r.buildMs = g.buildMs;
	r.note = why.isEmpty() ? QStringLiteral( "refused, and the builder gave no reason" ) : why;
	closeMessageBoxes();
	r.totalMs = g.cell.elapsed();
	r.rssMb = rssMb();
	check( !why.isEmpty(), QStringLiteral( "%1: a refusal carries its reason" ).arg( g.plan.at( ld.cells.first() ).key ) );
	g.refused++;
	QStringList lines;
	bool lead = true;
	for ( int pi : ld.cells ) {
		lines.append( rowLine( g.plan.at( pi ), r, lead ) );
		g.have.insert( g.plan.at( pi ).key );
		g.rows++;
		lead = false;
	}
	appendLines( g.censusPath, lines );
	closeLoad( r.totalMs );
}

/*! Turn a unit into the loads that walk it. A tile with nothing placed gets its
 *  rows here and no load at all. */
void prepareUnit( int ui )
{
	const CensusUnit & u = g.units.at( ui );
	QVector<int> need;
	for ( int pi : u.cells )
		if ( !g.have.contains( g.plan.at( pi ).key ) )
			need.append( pi );
	if ( need.isEmpty() ) {
		g.done++;
		return;
	}
	if ( u.interior ) {
		CensusLoad ld;
		ld.unit = ui;
		ld.cells = need;
		g.queue.append( ld );
		return;
	}
	const auto wi = g.worlds.find( u.world );
	if ( wi == g.worlds.end() ) {
		g.done++;
		return;
	}
	const EsmWorld & w = *wi->second;
	QElapsedTimer ct;
	ct.start();
	const int ring = 2 * g.margin;
	const qint64 n = blockRefs( w, u.tx, u.ty, g.block + ring );
	if ( n == 0 && !g.needScene ) {
		/* COUNT ONLY. Nothing is placed in what would be loaded, so every figure
		 * a row holds about placed objects is zero and is known from the plugin:
		 * the scene (terrain and water, nothing else) is not built for it. Only
		 * when no step needs the scene: a bake needs the ground. */
		CensusRow r;
		r.block = g.block + ring;
		r.steps = QStringLiteral( "census" );
		r.refs = r.refsDrawn = r.lightsCell = 0;
		r.refsBlock = 0;
		r.lightsBlock = 0;
		r.totalMs = ct.elapsed();
		r.note = QStringLiteral( "count only: nothing is placed in the tile, so the scene was not built" );
		QStringList lines;
		bool lead = true;
		for ( int pi : need ) {
			CensusRow cr = r;
			if ( !lead )
				cr.note.clear();
			lines.append( rowLine( g.plan.at( pi ), cr, lead ) );
			g.have.insert( g.plan.at( pi ).key );
			g.rows++;
			g.countOnly++;
			lead = false;
		}
		appendLines( g.censusPath, lines );
		g.done++;
		return;
	}
	const QString id = tileId( u.world, u.tx, u.ty );
	const bool died = g.splitTiles.contains( id );
	const bool over = ( g.refsMax > 0 && n > g.refsMax );
	if ( g.block > 1 && ( died || over ) ) {
		g.splitUnits++;
		for ( int pi : need ) {
			const CensusCell & c = g.plan.at( pi );
			CensusLoad ld;
			ld.unit = ui;
			ld.cells = { pi };
			ld.block = 1 + ring;
			ld.own = 1;
			ld.cx = c.x;
			ld.cy = c.y;
			ld.refs = blockRefs( w, c.x, c.y, 1 + ring );
			ld.note = over
				? QStringLiteral( "opened alone: the %1x%1 tile holds %2 references, over the walk's limit of %3" )
					.arg( g.block ).arg( n ).arg( g.refsMax )
				: QStringLiteral( "opened alone: the walk died on this tile as %1x%1" ).arg( g.block );
			g.queue.append( ld );
		}
		return;
	}
	CensusLoad ld;
	ld.unit = ui;
	ld.cells = need;
	ld.block = g.block + ring;
	ld.own = g.block;
	ld.cx = u.tx;
	ld.cy = u.ty;
	ld.refs = n;
	g.queue.append( ld );
}

void openNext()
{
	if ( g.finished )
		return;
	if ( g.queue.isEmpty() && g.todo.isEmpty() ) {
		finishWalk( QStringLiteral( "every cell named is in the census" ) );
		return;
	}
	if ( g.budgetS > 0 && g.wall.elapsed() > qint64( g.budgetS ) * 1000 ) {
		finishWalk( QStringLiteral( "the time budget of %1 s is spent" ).arg( g.budgetS ) );
		return;
	}
	/* the window does not hand back what a big load took (measured: 0.4 GB at
	 * the start, 13.8 GB twenty-one cells later), so a pass ends at a ceiling
	 * and the next pass, a new window, resumes. Never before one load is done. */
	if ( g.rssMaxMb > 0 && g.loads > 0 && rssMb() > double( g.rssMaxMb ) ) {
		finishWalk( QStringLiteral( "the memory ceiling of %1 MB is passed" ).arg( g.rssMaxMb ) );
		return;
	}
	if ( g.queue.isEmpty() ) {
		if ( g.maxUnits >= 0 && g.done >= g.maxUnits ) {
			finishWalk( QStringLiteral( "the unit limit of %1 is reached" ).arg( g.maxUnits ) );
			return;
		}
		prepareUnit( g.todo.takeFirst() );
		if ( g.queue.isEmpty() ) {
			// rows without a load (a tile with nothing placed): on to the next unit
			QTimer::singleShot( 0, g.skope, []() { openNext(); } );
			return;
		}
	}
	g.load = g.queue.takeFirst();
	const CensusLoad & ld = g.load;
	const CensusUnit & u = g.units.at( ld.unit );
	const QString leadKey = g.plan.at( ld.cells.first() ).key;
	{
		QFile sf( g.specPath );
		if ( sf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &sf );
			s << "# the cell census walk's scratch spec (lane PRTP5): " << leadKey << "\n";
			if ( u.interior )
				s << g.plugins << "|interior|" << hex8( g.plan.at( ld.cells.first() ).form ) << "\n";
			else
				s << g.plugins << "|" << u.world << "|" << ld.cx << "," << ld.cy << "|" << ld.block << "\n";
		}
	}
	{
		// what a walk that dies here died on: how many cells a side the load was for, and its cells
		QFile pf( g.pendingPath );
		if ( pf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &pf );
			s << "block " << ( u.interior ? 1 : ld.own ) << "\n";
			for ( int pi : ld.cells )
				s << g.plan.at( pi ).key << "\n";
		}
	}
	g.notes.clear();
	g.error.clear();
	markClean();
	// a Show row flipped in one load must not follow the walk into the next
	if ( cellWorkspaceHasOverrides() )
		cellWorkspaceForget();
	g.buildMs = 0;
	g.cell.start();
	g.loadStartMs = QDateTime::currentMSecsSinceEpoch();
	// every step says what the builder must do while it builds this load
	for ( const VisitStep * st : g.steps )
		if ( st->before )
			st->before( ld );
	g.waiting = true;
	QString path = g.specPath;
	if ( !g.skope->openFile( path ) ) {
		g.waiting = false;
		refuseLoad( QStringLiteral( "the window would not open the spec file" ) );
	}
}

void onLoaded( bool ok, const QString & fname )
{
	if ( !g.waiting )
		return;
	if ( QFileInfo( fname ).fileName() != QFileInfo( g.specPath ).fileName() )
		return;     // the starter scene, or anything else that is not the walk's
	g.waiting = false;
	g.buildMs = g.cell.elapsed();
	if ( !ok ) {
		const QString why = g.error;
		QTimer::singleShot( 0, g.skope, [why]() { refuseLoad( why ); } );
		return;
	}
	QTimer::singleShot( g.settleMs, g.skope, []() { measureLoad(); } );
}

void finishWalk( const QString & why )
{
	if ( g.finished )
		return;
	g.finished = true;
	const double runS = double( g.wall.elapsed() ) / 1000.0;
	const int left = g.todo.size() + ( g.queue.isEmpty() ? 0 : 1 );
	{
		QFile f( g.logPath );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &f );
			s << "# WW_CELL_CENSUS_TEST -- lane PRTP5\n";
			s << "census " << g.censusPath << "\n";
			s << "plan " << g.plan.size() << " cells: interiors " << g.planInteriors
			  << ", exterior " << g.planExteriors << ", exterior tile " << g.block << "x" << g.block
			  << "; units " << g.units.size() << "\n";
			if ( g.onlyNamed > 0 )
				s << "sample " << g.onlyNamed << " keys named, " << g.onlyUnknown << " not in the plan\n";
			s << "slice " << g.sliceI << " of " << g.sliceN << ": " << g.unitsMine << " of "
			  << g.unitsWanted << " units to walk\n";
			s << "already in the census " << g.already << "\n";
			s << "this run: " << g.done << " units in " << g.loads << " loads (ok " << g.ok << ", refused " << g.refused
			  << "), " << g.rows << " rows written (" << g.countOnly << " count only); tiles split " << g.splitUnits
			  << "; crashed rows written " << g.crashedRows << "; tiles a dead walk left to split " << g.diedTiles
			  << "; left " << left << " (" << why << ")\n";
			s << "seconds: run " << QString::number( runS, 'f', 1 )
			  << "; per interior " << ( g.nInterior ? QString::number( double( g.msInterior ) / 1000.0 / g.nInterior, 'f', 2 ) : QStringLiteral( "-" ) )
			  << " (" << g.nInterior << "); per exterior load "
			  << ( g.nExterior ? QString::number( double( g.msExterior ) / 1000.0 / g.nExterior, 'f', 2 ) : QStringLiteral( "-" ) )
			  << " (" << g.nExterior << " loads, " << g.nExteriorCells << " cells)\n";
			for ( const QString & l : g.failLines )
				s << "FAIL  " << l << "\n";
			if ( !g.bakeDir.isEmpty() )   // the gate reads this line: a passed check prints nothing above
				s << "bake version read back: " << g.versionRead << " visits, files .tbk v" << g.tbk << "\n";
			s << g.checks << " checks, " << g.failures << " failures\n";
			s << ( g.failures == 0 ? "PASS" : "FAIL" ) << "\n";
			s << "done\n";
		}
	}
	if ( g.skope ) {
		markClean();
		closeMessageBoxes();
	}
	if ( qgetenv( "WW_CELL_CENSUS_STAY" ).isEmpty() ) {
		QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
		// lane PRTP5: quit() is a request: it closes the windows first and a window may refuse, and asked
		// before the event loop runs it is lost. A window that found nothing left to load sat until its
		// timeout, 12 minutes of the NifSkope lock (2026-10-02 13:39). So every two seconds after that
		// the event loop is told to end without asking anybody, until it does.
		QTimer * again = new QTimer( qApp );
		QObject::connect( again, &QTimer::timeout, qApp, []() { QCoreApplication::exit( 0 ); } );
		again->start( 2000 );
	}
}

//! Every interior, then every exterior cell of each worldspace asked for; then the units.
bool buildPlan( QString * err )
{
	const bool dropRed = ( g.red == QLatin1String( "dropcell" ) );
	int seen = 0;
	auto add = [&]( const CensusCell & c ) {
		// RED `dropcell` (the gate's control): the walk quietly leaves cells out
		if ( dropRed && ( seen++ % 7 ) == 3 )
			return;
		g.plan.append( c );
		( c.interior ? g.planInteriors : g.planExteriors )++;
	};
	QString e;
	if ( qgetenv( "WW_CELL_CENSUS_NOINTERIORS" ).isEmpty() ) {
		const QVector<QPair<quint32, QString>> list = EsmWorld::listInteriors( g.plugins, &e );
		if ( !e.isEmpty() ) {
			*err = QStringLiteral( "the interiors could not be listed: %1" ).arg( e );
			return false;
		}
		for ( const auto & p : list ) {
			CensusCell c;
			c.interior = true;
			c.form = p.first;
			c.edid = p.second;
			c.key = QStringLiteral( "I:%1" ).arg( hex8( c.form ) );
			add( c );
		}
	}
	const QString worlds = envStr( "WW_CELL_CENSUS_WORLD", QStringLiteral( "Commonwealth" ) ).trimmed();
	if ( worlds.compare( QLatin1String( "none" ), Qt::CaseInsensitive ) != 0 ) {
		const QVector<QPair<quint32, QString>> wl = EsmWorld::listWorldspaces( g.plugins, &e );
		if ( !e.isEmpty() ) {
			*err = QStringLiteral( "the worldspaces could not be listed: %1" ).arg( e );
			return false;
		}
		const bool all = ( worlds.compare( QLatin1String( "all" ), Qt::CaseInsensitive ) == 0 );
		QStringList want = worlds.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
		for ( QString & w : want )
			w = w.trimmed();
		for ( const auto & ws : wl ) {
			if ( !all && !want.contains( ws.second, Qt::CaseInsensitive ) )
				continue;
			want.removeAll( ws.second );
			// kept for the walk: a tile's references are counted before it is opened
			std::unique_ptr<EsmWorld> & kept = g.worlds[ws.second];
			kept = std::make_unique<EsmWorld>();
			EsmWorld & w = *kept;
			if ( !w.load( g.plugins, ws.first, &e ) ) {
				*err = QStringLiteral( "worldspace %1 could not be read: %2" ).arg( ws.second, e );
				return false;
			}
			int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
			w.cellBounds( x0, y0, x1, y1 );
			for ( int y = y0; y <= y1; y++ ) {
				for ( int x = x0; x <= x1; x++ ) {
					if ( !w.hasCell( x, y ) )
						continue;
					CensusCell c;
					c.interior = false;
					c.world = ws.second;
					c.x = x;
					c.y = y;
					c.form = w.cellForm( x, y );
					c.edid = w.cellEditorId( x, y );
					c.key = QStringLiteral( "E:%1:%2,%3" ).arg( c.world ).arg( x ).arg( y );
					add( c );
				}
			}
		}
		if ( !all ) {
			for ( const QString & w : want ) {
				bool found = false;
				for ( const auto & ws : wl )
					found = found || ws.second.compare( w, Qt::CaseInsensitive ) == 0;
				if ( !found ) {
					*err = QStringLiteral( "no worldspace called \"%1\" in %2" ).arg( w, g.plugins );
					return false;
				}
			}
		}
	}

	// ---- the units: an interior each, and the exterior tiles, in the order their first cell is planned
	QHash<QString, int> tileUnit;
	for ( int i = 0; i < g.plan.size(); i++ ) {
		CensusCell & c = g.plan[i];
		if ( c.interior ) {
			CensusUnit u;
			u.cells.append( i );
			c.unit = g.units.size();
			g.units.append( u );
			continue;
		}
		const int tx = tileCentre( c.x, g.block ), ty = tileCentre( c.y, g.block );
		const QString id = tileId( c.world, tx, ty );
		auto it = tileUnit.constFind( id );
		if ( it == tileUnit.constEnd() ) {
			CensusUnit u;
			u.interior = false;
			u.world = c.world;
			u.tx = tx;
			u.ty = ty;
			it = tileUnit.insert( id, g.units.size() );
			g.units.append( u );
		}
		c.unit = it.value();
		g.units[it.value()].cells.append( i );
	}
	return true;
}

QSet<QString> keysOfFile( const QString & path, bool * existed = nullptr )
{
	QSet<QString> keys;
	QFile f( path );
	const bool open = f.open( QIODevice::ReadOnly | QIODevice::Text );
	if ( existed )
		*existed = open && f.size() > 0;
	if ( !open )
		return keys;
	QTextStream s( &f );
	while ( !s.atEnd() ) {
		const QString line = s.readLine();
		if ( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) || line.startsWith( QLatin1String( "key\t" ) ) )
			continue;
		const QString k = line.section( QLatin1Char( '\t' ), 0, 0 ).trimmed();
		if ( !k.isEmpty() )
			keys.insert( k );
	}
	return keys;
}

/*! The steps of a visit, from their names. `census` is always first: its rows
 *  are what a pass resumes from, whatever else the visit does. */
bool stepsFromNames( const QString & names, QString * err )
{
	QStringList want = names.toLower().split( QRegularExpression( QStringLiteral( "[,+ ]+" ) ), Qt::SkipEmptyParts );
	if ( !want.contains( QStringLiteral( "census" ) ) )
		want.prepend( QStringLiteral( "census" ) );
	QStringList done;
	for ( const VisitStep & st : kSteps ) {
		const QString n = QString::fromLatin1( st.name );
		if ( !want.contains( n ) )
			continue;
		want.removeAll( n );
		g.steps.append( &st );
		g.needScene = g.needScene || st.needsScene;
		done.append( n );
	}
	g.stepNames = done.join( QLatin1Char( '+' ) );
	if ( !want.isEmpty() ) {
		*err = QStringLiteral( "WW_CELL_CENSUS_STEPS names a step this walk does not have: %1" ).arg( want.join( QLatin1String( ", " ) ) );
		return false;
	}
	if ( done.contains( QStringLiteral( "bake" ) ) && g.bakeDir.isEmpty() ) {
		*err = QStringLiteral( "the bake step needs WW_CELL_CENSUS_BAKE, the folder it bakes into" );
		return false;
	}
	return true;
}

void startWalk()
{
	g.wall.start();
	g_prevHandler = qInstallMessageHandler( captureMessages );
	QString err;
	if ( !qgetenv( "WW_CELL_OPEN" ).isEmpty() )
		err = QStringLiteral( "WW_CELL_OPEN is set: it overrides every spec file, so every load of the walk would be the same cell" );
	else if ( g.plugins.isEmpty() )
		err = QStringLiteral( "WW_CELL_CENSUS_PLUGINS is not set" );
	else if ( g.block < 1 || ( g.block % 2 ) == 0 )
		err = QStringLiteral( "the tile size must be odd (1, 3, 5); got %1" ).arg( g.block );
	else if ( g.sliceN < 1 || g.sliceI < 1 || g.sliceI > g.sliceN )
		err = QStringLiteral( "WW_CELL_CENSUS_SLICE must be i/N with 1 <= i <= N; got %1/%2" ).arg( g.sliceI ).arg( g.sliceN );
	else if ( g.margin < 0 || g.margin > 4 )
		err = QStringLiteral( "the ring must be 0 to 4 cells; got %1" ).arg( g.margin );
	else if ( g.tbk != 3 && g.tbk != 4 )
		err = QStringLiteral( "the bake's file version must be 3 or 4; got %1" ).arg( g.tbk );
	else if ( !stepsFromNames( envStr( "WW_CELL_CENSUS_STEPS", QStringLiteral( "census" ) ), &err ) )
		;   // err says which step is not one
	else
		buildPlan( &err );
	if ( !err.isEmpty() ) {
		check( false, QStringLiteral( "the walk can start" ), err );
		finishWalk( QStringLiteral( "it never started" ) );
		return;
	}

	QHash<QString, int> byKey;
	for ( int i = 0; i < g.plan.size(); i++ )
		byKey.insert( g.plan.at( i ).key, i );

	// ---- the sample, if one is named: the units its keys are in
	QSet<QString> only;
	const QString onlyPath = envStr( "WW_CELL_CENSUS_ONLY" );
	if ( !onlyPath.isEmpty() ) {
		only = keysOfFile( onlyPath );
		g.onlyNamed = only.size();
		for ( const QString & k : only )
			if ( !byKey.contains( k ) )
				g.onlyUnknown++;
		check( !only.isEmpty(), QStringLiteral( "the sample file names cells" ), onlyPath );
		check( g.onlyUnknown == 0, QStringLiteral( "every sampled key is a cell of the plan" ),
			QStringLiteral( "%1 unknown" ).arg( g.onlyUnknown ) );
	}

	/* ---- THE SLICES. The wanted units are numbered in plan order, and unit u
	 * belongs to slice (u mod N) + 1: next-door tiles go to different walkers,
	 * so a heavy district is shared out.
	 * RED `dropslice` (the gate's control): the walkers count the slices one
	 * too many, so one unit in N + 1 belongs to nobody and every walker still
	 * says it is done.
	 * RED `doubleslice`: this walker also walks one unit in five that is not
	 * its own. */
	const bool dropSlice = ( g.red == QLatin1String( "dropslice" ) );
	const bool doubleSlice = ( g.red == QLatin1String( "doubleslice" ) );
	QVector<bool> mine( g.units.size(), false );
	for ( int ui = 0; ui < g.units.size(); ui++ ) {
		CensusUnit & u = g.units[ui];
		u.wanted = onlyPath.isEmpty();
		for ( int pi : u.cells )
			u.wanted = u.wanted || only.contains( g.plan.at( pi ).key );
		if ( !u.wanted )
			continue;
		u.order = g.unitsWanted++;
		u.slice = u.order % ( dropSlice ? g.sliceN + 1 : g.sliceN ) + 1;
		mine[ui] = ( u.slice == g.sliceI ) || ( doubleSlice && ( u.order % 5 ) == 2 );
		if ( mine.at( ui ) )
			g.unitsMine++;
	}

	if ( !g.planPath.isEmpty() ) {
		QFile pf( g.planPath );
		if ( pf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &pf );
			s << "# cell census plan (lane PRTP5): " << g.plugins << "; exterior tile " << g.block << "x" << g.block
			  << "; " << g.sliceN << " slices; columns: key, form, EDID, unit, place among the units walked, slice\n";
			for ( const CensusCell & c : g.plan ) {
				const CensusUnit & u = g.units.at( c.unit );
				s << c.key << "\t" << hex8( c.form ) << "\t" << ( c.edid.isEmpty() ? QStringLiteral( "-" ) : c.edid )
				  << "\t" << ( u.interior ? c.key : QStringLiteral( "T:%1" ).arg( tileId( u.world, u.tx, u.ty ) ) )
				  << "\t" << ( u.wanted ? QString::number( u.order ) : QStringLiteral( "-" ) )
				  << "\t" << ( u.wanted ? QString::number( u.slice ) : QStringLiteral( "-" ) ) << "\n";
			}
		}
	}

	// ---- what is already there, and what a dead walk died on
	QDir().mkpath( QFileInfo( g.censusPath ).absolutePath() );
	bool existed = false;
	g.have = keysOfFile( g.censusPath, &existed );
	if ( !existed ) {
		appendLines( g.censusPath, { QStringLiteral( "# WW cell census v2 (lane PRTP5): one row per cell; plugins=%1; exterior tile=%2x%2 "
			"(tiles do not overlap; `tile` is the tile's centre cell, `block` what the load was opened as, ring %6 included); "
			"slice %3 of %4; steps %7; "
			"refs, refs_drawn and lights_cell are the cell's own; refs_block to rss_mb are the whole load's, written on the "
			"load's first row and ^ on its other rows; build_ms is the whole load and bake_ms the part of it the bake took; "
			"far=none: nothing beyond the block is drawn; settle %5 ms is inside total_ms" )
			.arg( g.plugins ).arg( g.block ).arg( g.sliceI ).arg( g.sliceN ).arg( g.settleMs ).arg( g.margin )
			.arg( g.bakeDir.isEmpty() ? g.stepNames : QStringLiteral( "%1 (bake files .tbk v%2)" ).arg( g.stepNames ).arg( g.tbk ) ),
			QString::fromLatin1( kColumns ) } );
	}
	for ( const QString & t : keysOfFile( g.splitPath ) )
		g.splitTiles.insert( t );
	{
		QFile pf( g.pendingPath );
		if ( pf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
			QStringList lines = QString::fromUtf8( pf.readAll() ).split( QLatin1Char( '\n' ), Qt::SkipEmptyParts );
			pf.close();
			int diedBlock = 1;
			if ( !lines.isEmpty() && lines.first().startsWith( QLatin1String( "block " ) ) )
				diedBlock = lines.takeFirst().mid( 6 ).trimmed().toInt();
			QStringList crashed;
			for ( const QString & raw : lines ) {
				const QString k = raw.trimmed();
				if ( k.isEmpty() || g.have.contains( k ) || !byKey.contains( k ) )
					continue;
				const CensusCell & c = g.plan.at( byKey.value( k ) );
				const CensusUnit & u = g.units.at( c.unit );
				if ( !c.interior && diedBlock > 1 ) {
					// a tile: not lost, opened cell by cell from here on
					const QString id = tileId( u.world, u.tx, u.ty );
					if ( !g.splitTiles.contains( id ) ) {
						g.splitTiles.insert( id );
						appendLines( g.splitPath, { id } );
						g.diedTiles++;
					}
					continue;
				}
				CensusRow r;
				r.block = 1 + 2 * g.margin;
				r.steps = g.stepNames;
				r.status = QStringLiteral( "crashed" );
				r.note = QStringLiteral( "the walk died while this cell was open alone" );
				crashed.append( rowLine( c, r, true ) );
				g.have.insert( k );
				g.crashedRows++;
			}
			if ( !crashed.isEmpty() )
				appendLines( g.censusPath, crashed );
			QFile::remove( g.pendingPath );
		}
	}

	for ( int ui = 0; ui < g.units.size(); ui++ ) {
		if ( !mine.at( ui ) )
			continue;
		int lacking = 0;
		for ( int pi : g.units.at( ui ).cells ) {
			if ( g.have.contains( g.plan.at( pi ).key ) )
				g.already++;
			else
				lacking++;
		}
		if ( lacking > 0 )
			g.todo.append( ui );
	}
	if ( !g.shotDir.isEmpty() )
		QDir().mkpath( g.shotDir );

	g.skope->resize( 1280, 800 );
	flushTextures();
	openNext();
}

} // namespace

void wwCellCensusHarness( NifSkope * skope )
{
	static bool armed = false;
	QString census = envStr( "WW_CELL_CENSUS_TEST" );
	if ( census.isEmpty() || !skope || armed )
		return;
	armed = true;

	g.skope = skope;
	{
		// `i/N`: this walker's slice. With more than one slice it writes its own part file.
		const QStringList sl = envStr( "WW_CELL_CENSUS_SLICE", QStringLiteral( "1/1" ) ).trimmed().split( QLatin1Char( '/' ) );
		g.sliceI = sl.value( 0 ).trimmed().toInt();
		g.sliceN = sl.value( 1 ).trimmed().toInt();
	}
	QString part;
	census = QDir::fromNativeSeparators( census );
	if ( g.sliceN > 1 ) {
		part = QStringLiteral( ".part%1of%2" ).arg( g.sliceI ).arg( g.sliceN );
		if ( census.endsWith( QLatin1String( ".tsv" ), Qt::CaseInsensitive ) )
			census = census.left( census.size() - 4 ) + part + QStringLiteral( ".tsv" );
		else
			census += part;
	}
	g.censusPath = census;
	g.pendingPath = g.censusPath + QStringLiteral( ".pending" );
	g.splitPath = g.censusPath + QStringLiteral( ".split" );
	g.specPath = g.censusPath + QStringLiteral( ".walk.wwcell" );
	g.logPath = QCoreApplication::applicationDirPath() + QStringLiteral( "/ww_cell_census_test%1.log" ).arg( part );
	g.planPath = QDir::fromNativeSeparators( envStr( "WW_CELL_CENSUS_PLAN" ) );
	g.shotDir = QDir::fromNativeSeparators( envStr( "WW_CELL_CENSUS_SHOTS" ) );
	g.plugins = envStr( "WW_CELL_CENSUS_PLUGINS" ).trimmed();
	g.red = envStr( "WW_CELL_CENSUS_RED" ).trimmed().toLower();
	g.block = envInt( "WW_CELL_CENSUS_BLOCK", 5 );
	g.settleMs = qBound( 0, envInt( "WW_CELL_CENSUS_SETTLE_MS", 250 ), 60000 );
	g.budgetS = envInt( "WW_CELL_CENSUS_BUDGET", 480 );
	g.maxUnits = envInt( "WW_CELL_CENSUS_MAX", -1 );
	g.rssMaxMb = envInt( "WW_CELL_CENSUS_RSS_MAX", 8000 );
	g.refsMax = envInt( "WW_CELL_CENSUS_REFS_MAX", 12000 );
	g.margin = envInt( "WW_CELL_CENSUS_MARGIN", 0 );
	g.bakeDir = QDir::fromNativeSeparators( envStr( "WW_CELL_CENSUS_BAKE" ) );
	g.tbk = envInt( "WW_CELL_CENSUS_TBK", 4 );
	QFile::remove( g.logPath );

	QObject::connect( skope, &NifSkope::completeLoading, skope,
		[]( bool ok, QString & fname ) { onLoaded( ok, fname ); } );
	// after the window is up and the starter scene has had its turn
	QTimer::singleShot( 1500, skope, []() { startWalk(); } );
}
