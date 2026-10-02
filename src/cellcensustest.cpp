/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* ---------------------------------------------------------------------------
 * WW_CELL_CENSUS_TEST=<census file> -- lane PRTP5 (docs/PRTP_PLAN.md step 5,
 * "render each cell"), run INSIDE the running application.
 *
 * THE WALK. Every interior of the plugin and every exterior cell of the named
 * worldspace(s), ONE CELL AT A TIME in this one window: the cell is opened
 * through the same `.wwcell` door File > Open uses (a scratch spec file and
 * NifSkope::openFile), the Cell lights row is forced on, the frame is drawn
 * and grabbed, one row is appended to the census file, the texture cache is
 * flushed, and the next cell replaces it. The whole world is never loaded: an
 * exterior cell is opened as the block around it (WW_CELL_CENSUS_BLOCK, 5),
 * an interior whole and alone.
 *
 * RESUMABLE. Rows are APPENDED; a cell whose key is already in the file is
 * skipped. The key of the cell being opened sits in `<census>.pending` while
 * it is open, so a walk that died names the cell it died on: the next run
 * writes that cell a `crashed` row and goes on past it.
 *
 * THE ROW is what the builder itself said about the cell (its notes and its
 * refusal text, taken from the application's own "cell view:" log lines), the
 * reference model's count for the cell, the lights the shader was handed, and
 * what the texture cache was asked for and could not load. Nothing here is
 * judged from the picture except that a picture came back.
 *
 *   WW_CELL_CENSUS_PLUGINS   the load order, as a `.wwcell` names it (required)
 *   WW_CELL_CENSUS_WORLD     worldspace EDIDs, comma separated; `all`; `none`
 *                            (default Commonwealth)
 *   WW_CELL_CENSUS_NOINTERIORS=1   exteriors only
 *   WW_CELL_CENSUS_BLOCK     exterior block size, odd (default 5)
 *   WW_CELL_CENSUS_PLAN      write the whole walk plan here (key, form, EDID)
 *   WW_CELL_CENSUS_ONLY      a file of keys: walk only these (a sample)
 *   WW_CELL_CENSUS_BUDGET    seconds after which no new cell is started (480)
 *   WW_CELL_CENSUS_MAX       at most this many cells this run
 *   WW_CELL_CENSUS_RSS_MAX   MB of memory held after which no new cell is
 *                            started (8000; 0 = no ceiling)
 *   WW_CELL_CENSUS_REFS_MAX  an exterior block holding more references than
 *                            this is opened as 3x3, then as the cell alone,
 *                            and its row says so (12000; 0 = never shrink)
 *   WW_CELL_CENSUS_SETTLE_MS wait between the load and the frame (250)
 *   WW_CELL_CENSUS_SHOTS     a folder: `<key>.png` and `<key>.notes` per cell
 *   WW_CELL_CENSUS_RED       stale | dropcell -- the gate's red controls
 *   WW_CELL_DATAROOT         as for any cell (the builder reads it)
 *
 * WW_CELL_OPEN must NOT be set: it overrides every spec file, so every cell
 * of the walk would be the same cell. The harness refuses to start if it is.
 *
 * A harness FORCES the state it measures: the Cell lights row is switched on
 * here for every cell, and a row whose lights were not on fails its check.
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
};

//! What was counted for one cell. -1 = the builder did not say.
struct CensusRow
{
	QString status = QStringLiteral( "ok" );
	int block = 1;                  //!< the block the cell was opened as
	qint64 refs = -1, refsDrawn = -1, refsBlock = -1, placements = -1;
	qint64 shapes = -1, verts = -1, tris = -1;
	qint64 lightsCell = -1, lightsBlock = -1, lit = -1, omni = -1, spot = -1;
	qint64 skipOff = -1, skipNoRadius = -1, skipBlack = -1, ambientOnly = -1;
	QString lightTypes;
	qint64 lightsApprox = -1;       //!< hemisphere and box lights: read, drawn as plain omnis
	qint64 modelsLoaded = -1, modelsFailed = -1;
	int texAsked = -1, texMissing = -1;
	qint64 matsUnreadable = -1;
	qint64 coverPx = -1;
	QString fb;
	qint64 buildMs = -1, renderMs = -1, totalMs = -1;
	double rssMb = -1.0;
	QString note;
};

struct Walk
{
	NifSkope * skope = nullptr;
	QString censusPath, planPath, shotDir, logPath, specPath, pendingPath;
	QString plugins, red;
	int block = 5, settleMs = 250, budgetS = 480, maxCells = -1, rssMaxMb = 8000;
	QVector<CensusCell> plan;   //!< every cell of the walk, plugin order
	QVector<int> todo;          //!< plan indices still to do this run
	int planInteriors = 0, planExteriors = 0;
	int onlyNamed = 0, onlyUnknown = 0;
	int already = 0, at = -1;
	bool waiting = false, finished = false;
	int done = 0, ok = 0, refused = 0, crashedRows = 0;
	int checks = 0, failures = 0;
	QStringList failLines;
	QElapsedTimer wall, cell;
	qint64 buildMs = 0;
	qint64 msInterior = 0, msExterior = 0;
	int nInterior = 0, nExterior = 0;
	QString notes, error;       //!< the builder's own words for the cell being opened
	CensusRow prev;             //!< the last cell's true row (the stale red writes it again)
	bool havePrev = false;
	/* THE BLOCK A CELL IS OPENED AS. 5x5 is the ruling; a block that holds more
	 * references than refsMax is opened as 3x3, then as the cell alone, because
	 * the window's memory grows with the references it reads and a downtown
	 * block holds nine times what the Sanctuary one does. The row says so. */
	int refsMax = 12000, useBlock = 5;
	qint64 overRefs = -1;       //!< the asked-for block's count, when it was over
	std::map<QString, std::unique_ptr<EsmWorld>> worlds;
};

Walk g;
QtMessageHandler g_prevHandler = nullptr;

const char * const kColumns =
	"key\tkind\tworld\tx\ty\tform\tedid\tblock\tstatus\trefs\trefs_drawn\trefs_block\tplacements"
	"\tshapes\tverts\ttris\tlights_cell\tlights_block\tlit\tomni\tspot\tskip_off\tskip_noradius\tskip_black"
	"\tambient_only\tlight_types\tlights_approx\tmodels_loaded\tmodels_failed\ttex_asked\ttex_missing"
	"\tmats_unreadable\tfar\tcover_px\tfb\tbuild_ms\trender_ms\ttotal_ms\trss_mb\tnote";

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

//! One field of a row: no tabs, no line breaks.
QString flat( QString s )
{
	s.replace( QLatin1Char( '\t' ), QLatin1Char( ' ' ) );
	s.replace( QLatin1Char( '\r' ), QLatin1Char( ' ' ) );
	s.replace( QLatin1Char( '\n' ), QLatin1String( " / " ) );
	/* plain ASCII only: a base whose "model" is not a name (a placed armor's is
	 * a form id) reaches the notes as raw bytes, and the census is a text file */
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
 * thread, for the cell that is being opened, and everything else is passed on. */
void captureMessages( QtMsgType type, const QMessageLogContext & ctx, const QString & str )
{
	if ( g.waiting && str.startsWith( QLatin1String( "cell view:" ) )
		&& QThread::currentThread() == qApp->thread() ) {
		QString body = str.mid( 10 ).trimmed();
		if ( type == QtInfoMsg ) {
			g.notes = body;
			return;     // forty lines a cell would bury the run's log; the census is the record
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

void appendLine( const QString & path, const QString & line )
{
	QFile f( path );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream s( &f );
		s << line << "\n";
	}
}

void writeRow( const CensusCell & c, const CensusRow & r )
{
	QStringList f;
	auto n = [&f]( qint64 v ) { f.append( v < 0 ? QStringLiteral( "-" ) : QString::number( v ) ); };
	f << c.key << ( c.interior ? QStringLiteral( "interior" ) : QStringLiteral( "exterior" ) )
	  << ( c.interior ? QStringLiteral( "-" ) : c.world )
	  << ( c.interior ? QStringLiteral( "-" ) : QString::number( c.x ) )
	  << ( c.interior ? QStringLiteral( "-" ) : QString::number( c.y ) )
	  << hex8( c.form ) << ( c.edid.isEmpty() ? QStringLiteral( "-" ) : flat( c.edid ) )
	  << QString::number( c.interior ? 1 : r.block ) << r.status;
	n( r.refs ); n( r.refsDrawn ); n( r.refsBlock ); n( r.placements );
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
	n( r.buildMs ); n( r.renderMs ); n( r.totalMs );
	f << ( r.rssMb < 0.0 ? QStringLiteral( "-" ) : QString::number( r.rssMb, 'f', 0 ) );
	f << ( r.note.isEmpty() ? QStringLiteral( "-" ) : flat( r.note ) );
	appendLine( g.censusPath, f.join( QLatin1Char( '\t' ) ) );
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
	// one cell's textures at a time: the cache is not emptied by a new document
	if ( GLView * ogl = g.skope->getGLView() ) {
		ogl->makeCurrent();
		ogl->flush();
		ogl->doneCurrent();
	}
}

void finishWalk( const QString & why );
void openNext();

//! About how many references opening this block reads: each cell's own, summed.
qint64 blockRefs( const EsmWorld & w, int cx, int cy, int block )
{
	const int h = block / 2;
	qint64 n = 0;
	for ( int y = cy - h; y <= cy + h; y++ ) {
		for ( int x = cx - h; x <= cx + h; x++ ) {
			if ( w.hasCell( x, y ) )
				n += w.refrs( x, y ).size();
		}
	}
	return n;
}

void closeCell( const CensusCell & c, const CensusRow & r )
{
	QFile::remove( g.pendingPath );
	g.done++;
	( c.interior ? g.msInterior : g.msExterior ) += r.totalMs;
	( c.interior ? g.nInterior : g.nExterior )++;
	QTimer::singleShot( 0, g.skope, []() { openNext(); } );
}

//! The cell is built and its first frames are queued: draw it, read it, write its row.
void measureCell()
{
	const CensusCell & c = g.plan.at( g.at );
	NifSkope * skope = g.skope;
	GLView * ogl = skope->getGLView();
	NifModel * nif = skope->getNifModel();
	Scene * scene = ogl ? ogl->getScene() : nullptr;
	CensusRow r;
	r.buildMs = g.buildMs;
	r.block = c.interior ? 1 : g.useBlock;

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
		 * for, so from the second cell on it would not refit: center() asks
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

	// ---- the builder's notes
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

	// ---- the reference model: this cell's own count, and its own lights
	const CellRefTable & table = cellRefTable();
	const quint32 kLigh = quint32( 'L' ) | ( quint32( 'I' ) << 8 ) | ( quint32( 'G' ) << 16 ) | ( quint32( 'H' ) << 24 );
	int centre = -1;
	for ( int i = 0; i < table.cellCount(); i++ ) {
		const CellBlockEntry & e = table.cellAt( i );
		if ( c.interior ? ( e.cellForm == c.form ) : ( e.cx == c.x && e.cy == c.y ) )
			centre = i;
	}
	qint64 lightsTable = 0, lightsCentre = 0;
	for ( int i = 0; i < table.size(); i++ ) {
		const CellRefEntry & e = table.at( i );
		if ( e.baseType != kLigh || e.fate == CellRefFate::Deleted )
			continue;
		lightsTable++;
		if ( c.interior || ( e.cellX == c.x && e.cellY == c.y ) )
			lightsCentre++;
	}
	if ( centre >= 0 ) {
		r.refs = table.cellAt( centre ).references;
		r.refsDrawn = table.cellAt( centre ).drawn;
		r.lightsCell = lightsCentre;
	}

	// ---- the texture cache: what this cell's frames asked for
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
	if ( !c.interior && g.useBlock < g.block )
		note.append( QStringLiteral( "opened as %1x%1: the %2x%2 block holds about %3 references, over the walk's limit of %4" )
			.arg( g.useBlock ).arg( g.block ).arg( g.overRefs ).arg( g.refsMax ) );
	for ( const QString & line : g.notes.split( QLatin1Char( '\n' ) ) )
		if ( line.contains( QLatin1String( "REFUSED" ) ) )
			note.append( line.trimmed() );
	r.note = note.join( QLatin1String( "; " ) );

	// ---- the rows this run stands on
	const QString head = g.notes.section( QLatin1Char( '\n' ), 0, 0 );
	const QString wantHead = c.interior
		? QStringLiteral( "form 0x%1 " ).arg( hex8( c.form ).toLower() )
		: QStringLiteral( "cell view %1 %2,%3 block %4x%4 " ).arg( c.world ).arg( c.x ).arg( c.y ).arg( g.useBlock );
	check( head.contains( wantHead ), QStringLiteral( "%1: the builder's notes are for the cell asked for" ).arg( c.key ), head );
	check( centre >= 0 && table.cellAt( centre ).cellForm == c.form,
		QStringLiteral( "%1: the reference model holds this cell, by form" ).arg( c.key ),
		centre >= 0 ? hex8( table.cellAt( centre ).cellForm ) : QStringLiteral( "no such cell in the model" ) );
	check( wwCellLightsOn() && scene && wwCellLightsWanted( scene ),
		QStringLiteral( "%1: the Cell lights row is on for the frame" ).arg( c.key ) );
	{
		const WwCellLighting * L = nif ? wwCellLightsFor( nif ) : nullptr;
		check( L && qint64( L->lights.size() ) == r.lit,
			QStringLiteral( "%1: the lights handed to the shader are the ones the notes count" ).arg( c.key ),
			QStringLiteral( "%1 vs %2" ).arg( L ? L->lights.size() : -1 ).arg( r.lit ) );
	}
	check( lightsTable == r.lightsBlock,
		QStringLiteral( "%1: the reference model's placed lights equal the builder's lights line" ).arg( c.key ),
		QStringLiteral( "%1 vs %2" ).arg( lightsTable ).arg( r.lightsBlock ) );
	check( !fb.isNull() && ( r.shapes <= 0 || r.coverPx > 0 ),
		QStringLiteral( "%1: a frame came back, and it is not empty when shapes were drawn" ).arg( c.key ),
		QStringLiteral( "%1 cover %2 shapes %3" ).arg( r.fb ).arg( r.coverPx ).arg( r.shapes ) );

	if ( !g.shotDir.isEmpty() ) {
		QString base = c.key;
		base.replace( QLatin1Char( ':' ), QLatin1Char( '_' ) );
		base.replace( QLatin1Char( ',' ), QLatin1Char( '_' ) );
		if ( !fb.isNull() )
			fb.save( g.shotDir + QLatin1Char( '/' ) + base + QStringLiteral( ".png" ) );
		QFile nf( g.shotDir + QLatin1Char( '/' ) + base + QStringLiteral( ".notes" ) );
		if ( nf.open( QIODevice::WriteOnly | QIODevice::Text ) )
			nf.write( g.notes.toUtf8() );
	}

	flushTextures();
	r.totalMs = g.cell.elapsed();
	r.rssMb = rssMb();
	g.ok++;

	/* RED `stale` (the gate's control): the row carries the cell BEFORE it --
	 * what a one-window walk writes if it reads the window before the new cell
	 * has replaced the old one. Only an outside reader of the plugin can tell. */
	if ( g.red == QLatin1String( "stale" ) && g.havePrev ) {
		CensusRow s = g.prev;
		s.buildMs = r.buildMs;
		s.renderMs = r.renderMs;
		s.totalMs = r.totalMs;
		writeRow( c, s );
	} else {
		writeRow( c, r );
	}
	g.prev = r;
	g.havePrev = true;
	closeCell( c, r );
}

//! The builder refused the cell: the row carries its reason.
void refuseCell( const QString & why )
{
	const CensusCell & c = g.plan.at( g.at );
	CensusRow r;
	r.status = QStringLiteral( "refused" );
	r.block = c.interior ? 1 : g.useBlock;
	r.buildMs = g.buildMs;
	r.note = why.isEmpty() ? QStringLiteral( "refused, and the builder gave no reason" ) : why;
	closeMessageBoxes();
	r.totalMs = g.cell.elapsed();
	r.rssMb = rssMb();
	check( !why.isEmpty(), QStringLiteral( "%1: a refusal carries its reason" ).arg( c.key ) );
	g.refused++;
	writeRow( c, r );
	closeCell( c, r );
}

void openNext()
{
	if ( g.finished )
		return;
	if ( g.todo.isEmpty() ) {
		finishWalk( QStringLiteral( "every cell named is in the census" ) );
		return;
	}
	if ( g.budgetS > 0 && g.wall.elapsed() > qint64( g.budgetS ) * 1000 ) {
		finishWalk( QStringLiteral( "the time budget of %1 s is spent" ).arg( g.budgetS ) );
		return;
	}
	/* the window does not hand back what a big cell took (measured: 0.4 GB at
	 * the start, 13.8 GB twenty-one cells later), so a pass ends at a ceiling
	 * and the next pass, a new window, resumes. Never before one cell is done. */
	if ( g.rssMaxMb > 0 && g.done > 0 && rssMb() > double( g.rssMaxMb ) ) {
		finishWalk( QStringLiteral( "the memory ceiling of %1 MB is passed" ).arg( g.rssMaxMb ) );
		return;
	}
	if ( g.maxCells >= 0 && g.done >= g.maxCells ) {
		finishWalk( QStringLiteral( "the cell limit of %1 is reached" ).arg( g.maxCells ) );
		return;
	}
	g.at = g.todo.takeFirst();
	const CensusCell & c = g.plan.at( g.at );
	g.useBlock = g.block;
	g.overRefs = -1;
	if ( !c.interior && g.refsMax > 0 ) {
		const auto wi = g.worlds.find( c.world );
		while ( wi != g.worlds.end() && g.useBlock > 1 ) {
			const qint64 n = blockRefs( *wi->second, c.x, c.y, g.useBlock );
			if ( n <= g.refsMax )
				break;
			if ( g.overRefs < 0 )
				g.overRefs = n;
			g.useBlock -= 2;
		}
	}
	{
		QFile sf( g.specPath );
		if ( sf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &sf );
			s << "# the cell census walk's scratch spec (lane PRTP5): " << c.key << "\n";
			if ( c.interior )
				s << g.plugins << "|interior|" << hex8( c.form ) << "\n";
			else
				s << g.plugins << "|" << c.world << "|" << c.x << "," << c.y << "|" << g.useBlock << "\n";
		}
	}
	{
		QFile pf( g.pendingPath );
		if ( pf.open( QIODevice::WriteOnly | QIODevice::Text ) )
			pf.write( c.key.toUtf8() + "\n" );
	}
	g.notes.clear();
	g.error.clear();
	markClean();
	// a Show row flipped in one cell must not follow the walk into the next
	if ( cellWorkspaceHasOverrides() )
		cellWorkspaceForget();
	g.buildMs = 0;
	g.cell.start();
	g.waiting = true;
	QString path = g.specPath;
	if ( !g.skope->openFile( path ) ) {
		g.waiting = false;
		refuseCell( QStringLiteral( "the window would not open the spec file" ) );
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
		QTimer::singleShot( 0, g.skope, [why]() { refuseCell( why ); } );
		return;
	}
	QTimer::singleShot( g.settleMs, g.skope, []() { measureCell(); } );
}

void finishWalk( const QString & why )
{
	if ( g.finished )
		return;
	g.finished = true;
	const double runS = double( g.wall.elapsed() ) / 1000.0;
	{
		QFile f( g.logPath );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &f );
			s << "# WW_CELL_CENSUS_TEST -- lane PRTP5\n";
			s << "census " << g.censusPath << "\n";
			s << "plan " << g.plan.size() << " cells: interiors " << g.planInteriors
			  << ", exterior " << g.planExteriors << ", exterior block " << g.block << "\n";
			if ( g.onlyNamed > 0 )
				s << "sample " << g.onlyNamed << " keys named, " << g.onlyUnknown << " not in the plan\n";
			s << "already in the census " << g.already << "\n";
			s << "this run: " << g.done << " cells, ok " << g.ok << ", refused " << g.refused
			  << "; crashed rows written " << g.crashedRows << "; left " << g.todo.size()
			  << " (" << why << ")\n";
			s << "seconds: run " << QString::number( runS, 'f', 1 )
			  << "; per interior " << ( g.nInterior ? QString::number( double( g.msInterior ) / 1000.0 / g.nInterior, 'f', 2 ) : QStringLiteral( "-" ) )
			  << " (" << g.nInterior << "); per exterior "
			  << ( g.nExterior ? QString::number( double( g.msExterior ) / 1000.0 / g.nExterior, 'f', 2 ) : QStringLiteral( "-" ) )
			  << " (" << g.nExterior << ")\n";
			for ( const QString & l : g.failLines )
				s << "FAIL  " << l << "\n";
			s << g.checks << " checks, " << g.failures << " failures\n";
			s << ( g.failures == 0 ? "PASS" : "FAIL" ) << "\n";
			s << "done\n";
		}
	}
	if ( g.skope ) {
		markClean();
		closeMessageBoxes();
	}
	if ( qgetenv( "WW_CELL_CENSUS_STAY" ).isEmpty() )
		QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
}

//! Every interior, then every exterior cell of each worldspace asked for.
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
			// kept for the walk: openNext counts a block's references before it opens it
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

void startWalk()
{
	g.wall.start();
	g_prevHandler = qInstallMessageHandler( captureMessages );
	QString err;
	if ( !qgetenv( "WW_CELL_OPEN" ).isEmpty() )
		err = QStringLiteral( "WW_CELL_OPEN is set: it overrides every spec file, so every cell of the walk would be the same cell" );
	else if ( g.plugins.isEmpty() )
		err = QStringLiteral( "WW_CELL_CENSUS_PLUGINS is not set" );
	else if ( g.block < 1 || ( g.block % 2 ) == 0 )
		err = QStringLiteral( "the block size must be odd (1, 3, 5); got %1" ).arg( g.block );
	else
		buildPlan( &err );
	if ( !err.isEmpty() ) {
		check( false, QStringLiteral( "the walk can start" ), err );
		finishWalk( QStringLiteral( "it never started" ) );
		return;
	}

	if ( !g.planPath.isEmpty() ) {
		QFile pf( g.planPath );
		if ( pf.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream s( &pf );
			s << "# cell census plan (lane PRTP5): " << g.plugins << "\n";
			for ( const CensusCell & c : g.plan )
				s << c.key << "\t" << hex8( c.form ) << "\t" << ( c.edid.isEmpty() ? QStringLiteral( "-" ) : c.edid ) << "\n";
		}
	}

	// ---- what is already there, and the cell a dead walk died on
	QDir().mkpath( QFileInfo( g.censusPath ).absolutePath() );
	bool existed = false;
	QSet<QString> have = keysOfFile( g.censusPath, &existed );
	if ( !existed ) {
		appendLine( g.censusPath, QStringLiteral( "# WW cell census v1 (lane PRTP5): one row per cell; plugins=%1; exterior block=%2 (the block column is what a row was opened as); "
			"refs, refs_drawn and lights_cell are the cell's own, the *_block columns and the drawn counts are the whole block's; "
			"far=none: nothing beyond the block is drawn; settle %3 ms is inside total_ms" )
			.arg( g.plugins ).arg( g.block ).arg( g.settleMs ) );
		appendLine( g.censusPath, QString::fromLatin1( kColumns ) );
	}
	QHash<QString, int> byKey;
	for ( int i = 0; i < g.plan.size(); i++ )
		byKey.insert( g.plan.at( i ).key, i );
	{
		QFile pf( g.pendingPath );
		if ( pf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
			const QString k = QString::fromUtf8( pf.readLine() ).trimmed();
			pf.close();
			if ( !k.isEmpty() && !have.contains( k ) && byKey.contains( k ) ) {
				CensusRow r;
				r.block = g.block;
				r.status = QStringLiteral( "crashed" );
				r.note = QStringLiteral( "the walk died while this cell was open" );
				writeRow( g.plan.at( byKey.value( k ) ), r );
				have.insert( k );
				g.crashedRows++;
			}
			QFile::remove( g.pendingPath );
		}
	}

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
	for ( int i = 0; i < g.plan.size(); i++ ) {
		const QString & k = g.plan.at( i ).key;
		if ( !onlyPath.isEmpty() && !only.contains( k ) )
			continue;
		if ( have.contains( k ) ) {
			g.already++;
			continue;
		}
		g.todo.append( i );
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
	const QString census = envStr( "WW_CELL_CENSUS_TEST" );
	if ( census.isEmpty() || !skope || armed )
		return;
	armed = true;

	g.skope = skope;
	g.censusPath = QDir::fromNativeSeparators( census );
	g.pendingPath = g.censusPath + QStringLiteral( ".pending" );
	g.specPath = g.censusPath + QStringLiteral( ".walk.wwcell" );
	g.logPath = QCoreApplication::applicationDirPath() + QStringLiteral( "/ww_cell_census_test.log" );
	g.planPath = QDir::fromNativeSeparators( envStr( "WW_CELL_CENSUS_PLAN" ) );
	g.shotDir = QDir::fromNativeSeparators( envStr( "WW_CELL_CENSUS_SHOTS" ) );
	g.plugins = envStr( "WW_CELL_CENSUS_PLUGINS" ).trimmed();
	g.red = envStr( "WW_CELL_CENSUS_RED" ).trimmed().toLower();
	g.block = envInt( "WW_CELL_CENSUS_BLOCK", 5 );
	g.settleMs = qBound( 0, envInt( "WW_CELL_CENSUS_SETTLE_MS", 250 ), 60000 );
	g.budgetS = envInt( "WW_CELL_CENSUS_BUDGET", 480 );
	g.maxCells = envInt( "WW_CELL_CENSUS_MAX", -1 );
	g.rssMaxMb = envInt( "WW_CELL_CENSUS_RSS_MAX", 8000 );
	g.refsMax = envInt( "WW_CELL_CENSUS_REFS_MAX", 12000 );
	QFile::remove( g.logPath );

	QObject::connect( skope, &NifSkope::completeLoading, skope,
		[]( bool ok, QString & fname ) { onLoaded( ok, fname ); } );
	// after the window is up and the starter scene has had its turn
	QTimer::singleShot( 1500, skope, []() { startWalk(); } );
}
