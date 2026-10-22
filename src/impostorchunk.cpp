/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "impostorchunk.h"

#include "gl/glscene.h"
#include "model/nifmodel.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

/* ---------------------------------------------------------------------------
 * See impostorchunk.h for what this is and why it ships OFF.
 *
 * THE MANIFEST IS PARSED HERE IN ONE PASS, not by calling
 * `impostorReadManifest` and then reading the file a second time for the
 * object rows. The two kinds of line are PAIRED BY ADJACENCY as well as by
 * index -- lodgen writes each `C` line directly under the object row it
 * belongs to -- and a reader that makes two independent passes has to invent a
 * rule for what to do when the indices disagree. One pass cannot disagree with
 * itself: the `C` line takes the object row above it, and its own `index`
 * token is CHECKED against that row rather than trusted, so a manifest written
 * by some future tool in another order is refused loudly instead of drawing
 * trees in the wrong places.
 * ------------------------------------------------------------------------- */

namespace
{

struct ChunkState
{
	QVector<ImpostorChunk::Placed> placed;
	QVector<ImpostorCardSet> sets;		//!< one per distinct `.lodm`
	QStringList lodmKeys;				//!< parallel to `sets`
	QStringList notes;
	QString chunkDir;
	bool sheetsRegistered = false;
	//! Armed because the person opened a `.lodm`, not because a chunk placed
	//! it. Such a card draws with the master off -- see `armSingle`.
	bool document = false;
};

ChunkState & st()
{
	static ChunkState s;
	return s;
}

const char * kSettingsKey = "GLView/ImpostorChunkCards";

/*! Where the `.lodm` a `C` line names actually is.
 *
 *  The manifest writes a GAME path (`Data\FO4CSLOD\Cards\<id>_oct.lodm`), and
 *  the same set can be beside the chunk, under a data root, or inside an
 *  archive. The order below is cheapest-and-most-local first, and each step is
 *  a place the file has actually been found during this lane rather than a
 *  guess: `bake_fixture.sh` leaves it in `<chunkdir>/cards/`, a real lodgen run
 *  leaves it under the data root the run was given.
 *
 *  Returns empty when none of them has it, and the caller SAYS SO -- a card
 *  that silently does not appear is the failure this lane exists to end.
 */
QString resolveLodm( NifModel * nif, const QString & chunkDir, const QString & gamePath )
{
	QString rel = gamePath;
	rel.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	const QString base = QFileInfo( rel ).fileName();

	QStringList tries;
	tries << rel;								// already absolute, or cwd-relative
	tries << chunkDir + QLatin1Char( '/' ) + base;
	tries << chunkDir + QStringLiteral( "/cards/" ) + base;
	tries << chunkDir + QLatin1Char( '/' ) + rel;
	tries << QFileInfo( chunkDir ).absolutePath() + QLatin1Char( '/' ) + rel;
	for ( const QString & t : tries ) {
		if ( !t.isEmpty() && QFileInfo::exists( t ) )
			return QFileInfo( t ).absoluteFilePath();
	}
	if ( nif ) {
		const QString found = nif->findResourceFile( gamePath, "", ".lodm" );
		if ( !found.isEmpty() )
			return found;
	}
	return QString();
}

} // namespace

bool ImpostorChunk::enabled()
{
	/* THE ENVIRONMENT WINS, and only for a measuring run. The gate must be able
	 * to switch the master on without writing to the person's QSettings -- a
	 * harness that persists the state it forced is the known cause of the
	 * standing `native_open.sh` red. */
	if ( qEnvironmentVariableIsSet( "WW_IMPOSTOR_CHUNK" ) )
		return qEnvironmentVariableIntValue( "WW_IMPOSTOR_CHUNK" ) != 0;
	return QSettings().value( QLatin1String( kSettingsKey ), false ).toBool();
}

void ImpostorChunk::setEnabled( bool on )
{
	QSettings().setValue( QLatin1String( kSettingsKey ), on );
}

void ImpostorChunk::forget()
{
	st() = ChunkState();
}

const QVector<ImpostorChunk::Placed> & ImpostorChunk::placements()
{
	return st().placed;
}

QStringList ImpostorChunk::report()
{
	return st().notes;
}

int ImpostorChunk::arm( NifModel * nif, const QString & chunkPath )
{
	ChunkState & s = st();
	s = ChunkState();
	if ( chunkPath.isEmpty() )
		return 0;

	const QString manifest = chunkPath + QStringLiteral( ".manifest.txt" );
	s.chunkDir = QFileInfo( chunkPath ).absolutePath();
	if ( !QFileInfo::exists( manifest ) ) {
		// NOT an error. Most chunks have no manifest, and a chunk opened to be
		// looked at is not asking for cards.
		return 0;
	}

	QFile f( manifest );
	if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		s.notes << QStringLiteral( "impostor chunk: cannot read %1" ).arg( manifest );
		return 0;
	}

	QTextStream in( &f );
	// The object row most recently seen, which the next `C` line belongs to.
	int rowIndex = -1;
	float rowPos[3] = { 0.0f, 0.0f, 0.0f };
	float rowScale = 1.0f;
	QString rowForm;
	int cLines = 0, refused = 0;

	while ( !in.atEnd() ) {
		const QString line = in.readLine();
		if ( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) )
			continue;

		if ( line.startsWith( QLatin1String( "C " ) ) ) {
			cLines++;
			const ImpostorPlacement c = impostorParseCLine( line );
			if ( !c.ok ) {
				refused++;
				if ( refused <= 4 )
					s.notes << QStringLiteral( "impostor chunk: C line refused -- %1" ).arg( c.error );
				continue;
			}
			if ( c.index != rowIndex ) {
				refused++;
				if ( refused <= 4 )
					s.notes << QStringLiteral( "impostor chunk: C line index %1 does not match the"
							" object row above it (%2) -- refused rather than placed by guess" )
							.arg( c.index ).arg( rowIndex );
				continue;
			}
			Placed p;
			p.c = c;
			p.world[0] = rowPos[0]; p.world[1] = rowPos[1]; p.world[2] = rowPos[2];
			p.scale = rowScale;
			p.formId = rowForm;
			s.placed.append( p );
			continue;
		}

		// An object row: index formid TYPE x y z scale class height ref part
		const QStringList t = line.split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
		if ( t.size() < 7 )
			continue;
		bool okI = false, okX = false, okY = false, okZ = false, okS = false;
		const int idx = t.at( 0 ).toInt( &okI );
		const float x = t.at( 3 ).toFloat( &okX );
		const float y = t.at( 4 ).toFloat( &okY );
		const float z = t.at( 5 ).toFloat( &okZ );
		const float sc = t.at( 6 ).toFloat( &okS );
		if ( !( okI && okX && okY && okZ && okS ) )
			continue;
		rowIndex = idx;
		rowPos[0] = x; rowPos[1] = y; rowPos[2] = z;
		rowScale = ( sc > 0.0f ) ? sc : 1.0f;
		rowForm = t.at( 1 );
	}

	// The distinct sets, loaded once each. A chunk of a hundred maples names
	// one `.lodm` a hundred times and this is not a hundred loads.
	for ( Placed & p : s.placed ) {
		const QString key = p.c.lodm.toLower();
		int at = s.lodmKeys.indexOf( key );
		if ( at < 0 ) {
			const QString path = resolveLodm( nif, s.chunkDir, p.c.lodm );
			ImpostorCardSet set;
			if ( path.isEmpty() ) {
				set.ok = false;
				set.error = QStringLiteral( "not found beside the chunk or on the resource stack" );
				s.notes << QStringLiteral( "impostor chunk: %1 -- %2" ).arg( p.c.lodm, set.error );
			} else {
				set = impostorCardLoad( path, p.c.arrayLodm.isEmpty() ? -1 : p.c.layer );
				if ( !set.ok )
					s.notes << QStringLiteral( "impostor chunk: %1 -- %2" ).arg( path, set.error );
			}
			s.lodmKeys << key;
			s.sets << set;
			at = s.sets.size() - 1;
		}
		p.setIndex = s.sets.at( at ).ok ? at : -1;
	}

	int drawable = 0, gridsSeen = 0;
	QList<int> grids;
	for ( const Placed & p : s.placed ) {
		if ( p.setIndex >= 0 )
			drawable++;
	}
	for ( const ImpostorCardSet & set : s.sets ) {
		if ( set.ok && !grids.contains( set.oct ) ) {
			grids << set.oct;
			gridsSeen++;
		}
	}
	QStringList gridWords;
	for ( int g : grids )
		gridWords << QString::number( g );
	s.notes.prepend( QStringLiteral( "impostor chunk: %1 C lines, %2 placed, %3 drawable,"
			" %4 distinct set%5, grid%6 %7 -- master %8" )
			.arg( cLines ).arg( s.placed.size() ).arg( drawable ).arg( s.sets.size() )
			.arg( s.sets.size() == 1 ? QString() : QStringLiteral( "s" ) )
			.arg( gridsSeen == 1 ? QString() : QStringLiteral( "s" ),
				gridWords.isEmpty() ? QStringLiteral( "none" ) : gridWords.join( QLatin1Char( ',' ) ),
				enabled() ? QStringLiteral( "ON" ) : QStringLiteral( "OFF (ships off)" ) ) );
	return s.placed.size();
}

bool ImpostorChunk::armSingle( NifModel * nif, const QString & lodmPath, QString * error )
{
	ChunkState & s = st();
	s = ChunkState();
	s.chunkDir = QFileInfo( lodmPath ).absolutePath();

	const ImpostorCardSet set = impostorCardLoad( lodmPath, -1 );
	if ( !set.ok ) {
		if ( error )
			*error = set.error;
		return false;
	}
	s.sets << set;
	s.lodmKeys << lodmPath.toLower();

	Placed p;
	p.c.ok = true;
	p.c.index = 0;
	p.c.center[0] = set.center[0];
	p.c.center[1] = set.center[1];
	p.c.center[2] = set.center[2];
	p.c.halfW = set.halfW;
	p.c.halfH = set.halfH;
	p.c.oct = set.oct;
	p.c.depthSpan = set.depthSpan;
	p.c.lodm = lodmPath;
	p.scale = 1.0f;
	p.setIndex = 0;
	s.placed << p;
	s.document = true;
	s.notes = set.notes();
	s.notes.prepend( QStringLiteral( "impostor card document: %1" ).arg( lodmPath ) );
	(void) nif;
	return true;
}

int ImpostorChunk::draw( Scene * scene, const ImpostorDraw::Options & opt )
{
	ChunkState & s = st();
	if ( !scene || s.placed.isEmpty() )
		return 0;
	// The master gates a CHUNK's cards. A `.lodm` the person opened is the
	// document itself and draws either way -- see `armSingle`.
	if ( !s.document && !enabled() )
		return 0;

	/* THE SHEETS FIRST, ALWAYS, AND ONCE. `TexCache` remembers a failure: a
	 * bind attempted before the set's own folder is on the resource list leaves
	 * the entry poisoned for the rest of the session, however correct the path
	 * becomes a frame later (gltex.cpp:337..341). */
	if ( !s.sheetsRegistered ) {
		s.sheetsRegistered = true;
		for ( const ImpostorCardSet & set : s.sets ) {
			if ( !set.ok )
				continue;
			const QString root = ImpostorDraw::registerLooseSheets( scene, set );
			if ( !root.isEmpty() )
				s.notes << QStringLiteral( "impostor chunk: registered %1" ).arg( root );
		}
		/* AND THEN DRAW, in this same frame. The cache is only poisoned by a
		 * bind attempted BEFORE the folder is on the resource list, and the
		 * registration above has just finished; skipping a frame here cost a
		 * blank picture on every one-frame render (`WW_RENDER_SHOT` grabs and
		 * exits), which is the one case where there is no second frame. */
	}

	/* THE COUNT, SAID ONCE, ON stdout. A picture of a chunk full of trees does
	 * not tell anyone whether the cards are drawn or whether they are the
	 * chunk's own LOD shapes seen through them, and a gate cannot read a
	 * picture. This line is what `tests/spells/impostor_draw.sh` row 12 reads,
	 * and it is printed only when the number CHANGES, so a 60 Hz viewport does
	 * not fill the log with it. */
	static int saidDrawn = -1;

	int drawn = 0;
	for ( const Placed & p : s.placed ) {
		if ( p.setIndex < 0 || p.setIndex >= s.sets.size() )
			continue;
		const ImpostorCardSet & set = s.sets.at( p.setIndex );
		ImpostorDraw::Options o = opt;
		o.worldScale = p.scale;
		QString why;
		if ( ImpostorDraw::drawCard( scene, set, Vector3( p.world[0], p.world[1], p.world[2] ),
									 o, &why ) ) {
			drawn++;
		} else {
			// Once per distinct reason, not once per card per frame.
			static QString said;
			if ( said != why ) {
				said = why;
				s.notes << QStringLiteral( "impostor chunk: draw refused -- %1" ).arg( why );
			}
		}
	}
	if ( drawn != saidDrawn ) {
		saidDrawn = drawn;
		qInfo().noquote() << QStringLiteral( "impostor chunk: drew %1 cards of %2 placed" )
				.arg( drawn ).arg( s.placed.size() );
	}
	return drawn;
}
