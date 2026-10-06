/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "impostorchunk.h"

#include "gl/glscene.h"
#include "gl/renderer.h"
#include "lodgen.h"
#include "model/nifmodel.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

#include <cmath>

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

//! lane FARLOD1: the cell view's far cards (see impostorchunk.h)
struct CellFarState
{
	ChunkState cards;
	float shift[3] = { 0.0f, 0.0f, 0.0f };
};
CellFarState & farSt()
{
	static CellFarState s;
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
	/* FINALFIX: the generator's resource stack (WW_LODGEN_RESOURCES / the LOD panel), each root taken as a
	 * Data folder -- the far field's cards live where the chunks' authored game path says, under any root */
	{
		QString inData = rel;
		if ( inData.startsWith( QLatin1String( "Data/" ), Qt::CaseInsensitive ) )
			inData = inData.mid( 5 );
		// the stack's own entries, the LAST one wins (Mod Organizer's order); a search path is an entry's
		// Meshes/Textures/... folder, so its parent is a Data root too
		QStringList roots;
		const QStringList stack = lodgenResources();
		for ( int i = stack.size() - 1; i >= 0; i-- )
			roots << QDir::cleanPath( stack.at( i ) );
		for ( const QString & p : lodgenResourceSearchPaths() )
			roots << QDir::cleanPath( p + QStringLiteral( "/.." ) );
		for ( const QString & root : roots ) {
			if ( !QFileInfo( root ).isDir() )
				continue;
			const QString t = QDir( root ).filePath( inData );
			if ( QFileInfo::exists( t ) )
				return QFileInfo( t ).absoluteFilePath();
		}
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
	QString rowForm, rowRef;
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
			p.ref = rowRef;
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
		rowRef = t.size() > 9 ? t.at( 9 ) : QString();
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

/* ---- lane FARLOD1: the cell view's far tree cards (impostorchunk.h) ---- */
int ImpostorChunk::armCellFar( NifModel * nif, const QStringList & chunkPaths, const float shift[3],
	float cx, float cy, float rMax, int hx0, int hy0, int hx1, int hy1,
	QSet<quint32> * refs, QStringList * notes )
{
	CellFarState & F = farSt();
	F = CellFarState();
	F.shift[0] = shift[0];
	F.shift[1] = shift[1];
	F.shift[2] = shift[2];

	// the chunk state is borrowed per manifest and put back as it was found
	const ChunkState saved = st();
	int read = 0, kept = 0, inHole = 0, beyond = 0;
	for ( const QString & chunk : chunkPaths ) {
		const int got = arm( nif, chunk );
		if ( got <= 0 ) {
			if ( notes )
				for ( const QString & n : std::as_const( st().notes ) )
					if ( n.contains( QLatin1String( "cannot" ) ) )
						*notes << n;
			continue;
		}
		read += got;
		ChunkState & s = st();
		for ( const Placed & p0 : std::as_const( s.placed ) ) {
			const int cellX = int( std::floor( p0.world[0] / 4096.0f ) );
			const int cellY = int( std::floor( p0.world[1] / 4096.0f ) );
			if ( cellX >= hx0 && cellX <= hx1 && cellY >= hy0 && cellY <= hy1 ) {
				inHole++;
				continue;
			}
			const float dx = p0.world[0] - cx, dy = p0.world[1] - cy;
			if ( rMax > 0.0f && dx * dx + dy * dy > rMax * rMax ) {
				beyond++;
				continue;
			}
			Placed p = p0;
			if ( p0.setIndex >= 0 && p0.setIndex < s.sets.size() ) {
				// one set per distinct .lodm across every chunk
				const QString key = s.lodmKeys.at( p0.setIndex );
				int at = F.cards.lodmKeys.indexOf( key );
				if ( at < 0 ) {
					F.cards.lodmKeys << key;
					F.cards.sets << s.sets.at( p0.setIndex );
					at = F.cards.sets.size() - 1;
				}
				p.setIndex = at;
			} else {
				p.setIndex = -1;
			}
			F.cards.placed << p;
			kept++;
			if ( refs && !p.ref.isEmpty() ) {
				bool ok = false;
				const quint32 id = p.ref.toUInt( &ok, 16 );
				if ( ok )
					refs->insert( id );
			}
		}
		if ( F.cards.chunkDir.isEmpty() )
			F.cards.chunkDir = s.chunkDir;
		if ( notes )
			for ( const QString & n : std::as_const( s.notes ) )
				if ( n.contains( QLatin1String( "refused" ) ) || n.contains( QLatin1String( "not found" ) )
					|| n.contains( QLatin1String( "cannot" ) ) )
					*notes << n;
	}
	st() = saved;
	if ( notes )
		*notes << QStringLiteral( "far lod: cards %1 read from %2 chunk manifests, %3 kept, %4 in the hole, "
			"%5 past %6 units, %7 sets" ).arg( read ).arg( chunkPaths.size() ).arg( kept ).arg( inHole )
			.arg( beyond ).arg( double( rMax ), 0, 'f', 0 ).arg( F.cards.sets.size() );
	return kept;
}

int ImpostorChunk::drawCellFar( Scene * scene, const ImpostorDraw::Options & opt )
{
	CellFarState & F = farSt();
	ChunkState & s = F.cards;
	if ( !scene || s.placed.isEmpty() )
		return 0;
	if ( !s.sheetsRegistered ) {
		s.sheetsRegistered = true;
		for ( const ImpostorCardSet & set : std::as_const( s.sets ) )
			if ( set.ok )
				ImpostorDraw::registerLooseSheets( scene, set );
	}
	static int saidDrawn = -1, saidOut = -1;
	int drawn = 0, outside = 0;
	/* THE CAMERA'S CULL (lane FARLOD1): a card whose bounding sphere is outside one of the four side planes
	 * is not drawn, the game's per-object test (SUNCELL1's cellcull uses the same planes for the document's
	 * shapes). Cards cast no sun shadow (they are no document shape), so no cascade asks for them.
	 * WW_CELL_FARLOD_RED=nocardcull draws every card: the picture must not change, the count must. */
	float pl[4][4];
	bool cull = qgetenv( "WW_CELL_FARLOD_RED" ) != "nocardcull" && scene->renderer
		&& scene->renderer->globalUniforms->projectionMatrix[3][3] != 1.0f;
	if ( cull ) {
		const auto & pm = scene->renderer->globalUniforms->projectionMatrix;	// pm[column][row]
		for ( int i = 0; i < 4; i++ ) {
			const int axis = i >> 1;
			const float sg = ( i & 1 ) ? -1.0f : 1.0f;
			float l = 0.0f;
			for ( int c = 0; c < 4; c++ ) {
				pl[i][c] = pm[c][3] + sg * pm[c][axis];
				if ( c < 3 )
					l += pl[i][c] * pl[i][c];
			}
			l = std::sqrt( l );
			if ( l > 0.0f )
				for ( int c = 0; c < 4; c++ )
					pl[i][c] /= l;
		}
	}
	for ( const Placed & p : std::as_const( s.placed ) ) {
		if ( p.setIndex < 0 || p.setIndex >= s.sets.size() )
			continue;
		if ( cull ) {
			const float sc = std::fabs( p.scale );
			const Vector3 cw( p.world[0] - F.shift[0] + p.c.center[0] * sc, p.world[1] - F.shift[1] + p.c.center[1] * sc,
				p.world[2] - F.shift[2] + p.c.center[2] * sc );
			const Vector3 v = scene->view * cw;
			const float r = sc * std::sqrt( p.c.halfW * p.c.halfW + p.c.halfH * p.c.halfH ) * 1.5f + 64.0f;
			bool in = true;
			for ( int i = 0; i < 4 && in; i++ )
				in = pl[i][0] * v[0] + pl[i][1] * v[1] + pl[i][2] * v[2] + pl[i][3] >= -r;
			if ( !in ) {
				outside++;
				continue;
			}
		}
		ImpostorDraw::Options o = opt;
		o.worldScale = p.scale;
		if ( o.swayAmplitude != 0.0f )   // a forest does not sway as one
			o.swayPhase += 0.0021f * p.world[0] + 0.0017f * p.world[1];
		QString why;
		if ( ImpostorDraw::drawCard( scene, s.sets.at( p.setIndex ),
				Vector3( p.world[0] - F.shift[0], p.world[1] - F.shift[1], p.world[2] - F.shift[2] ), o, &why ) )
			drawn++;
	}
	if ( drawn != saidDrawn || outside != saidOut ) {
		saidDrawn = drawn;
		saidOut = outside;
		qInfo().noquote() << QStringLiteral( "far lod: drew %1 cards of %2 placed, %3 outside the camera%4" )
			.arg( drawn ).arg( s.placed.size() ).arg( outside )
			.arg( cull ? QString() : QStringLiteral( " (cull off%1)" )
				.arg( qgetenv( "WW_CELL_FARLOD_RED" ) == "nocardcull" ? QStringLiteral( ", RED nocardcull" ) : QString() ) );
	}
	return drawn;
}

void ImpostorChunk::forgetCellFar()
{
	farSt() = CellFarState();
}

int ImpostorChunk::cellFarCount()
{
	return farSt().cards.placed.size();
}
