/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "impostorchunk.h"
#include "wwswitch.h"	// the WW_* switch store
#include "lodinative.h"	// lane PERFLOAD1 P1: LodiTreeCard
#include "cellfarhide.h"	// lane PERFLOAD1 P7: the ladder hides mesh rows by REFR
#include "treeladder.h"
#include "lodmfile.h"
#include "recx.h"	// lane PERFLOAD1 P1: wwAssetsModRoot, where the shipped impostor sets live

#include "gl/glscene.h"
#include "gl/renderer.h"
#include "lodgen.h"
#include "model/nifmodel.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QTextStream>

#include <cmath>

// See impostorchunk.h for what this is and why it ships OFF:
// THE MANIFEST IS PARSED HERE IN ONE PASS, not by calling `impostorReadManifest` and then reading the file a
// second time for the object rows.

bool wwFrameLog();	// WW_FRAME_LOG=0 keeps the per-camera far LOD lines out

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

//! the cell view's far cards (see impostorchunk.h)
struct CellFarState
{
	ChunkState cards;
	float shift[3] = { 0.0f, 0.0f, 0.0f };
	QHash<quint32, int> treeSet;	//!< lane PERFLOAD1 P1: base form -> its set in cards.sets (cellFarTreeForms)
	//! lane PERFLOAD1 P7: per placement its ladder step and fade, and whether its mesh row is hidden now
	QVector<TreeLadder::State> ladder;
	QVector<char> ladderHid;
	quint64 ladderFor = ~quint64( 0 );
};
// cells a streamed near cell holds; their cards do not draw. Kept apart from CellFarState so a far
// rebuild (forgetCellFar + arm) keeps the stream's cells. hideGen bumps on each change (the shadow casters rebuild).
struct CardHide
{
	QSet<qint64> cells;
	quint64 gen = 0;
};
CardHide & cardHide()
{
	static CardHide h;
	return h;
}
qint64 cardCell( const ImpostorChunk::Placed & p )
{
	return ImpostorChunk::cellFarCellKey( int( std::floor( p.world[0] / 4096.0f ) ), int( std::floor( p.world[1] / 4096.0f ) ) );
}
bool cardHidden( const ImpostorChunk::Placed & p )
{
	const CardHide & h = cardHide();
	return !h.cells.isEmpty() && h.cells.contains( cardCell( p ) );
}
CellFarState & farSt()
{
	static CellFarState s;
	return s;
}
//! bumped on every arm and forget, so the instanced pass rebuilds its frame tables
quint64 & farGenRef()
{
	static quint64 g = 0;
	return g;
}
quint64 farGen()
{
	return farGenRef();
}

const char * kSettingsKey = "GLView/ImpostorChunkCards";

//! Where the `.lodm` a `C` line names actually is:
//! The manifest writes a GAME path (`Data\FO4CSLOD\Cards\<id>_oct.lodm`), and the same set can be beside the
//! chunk, under a data root, or inside an archive.
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
	/* a Data-rooted game path ("data\FO4CSLOD\Commonwealth\Objects\<atlas>.lodm", the C line's array
	 * atlas) resolves against the Data root the chunk itself sits in: the chunk folder's ancestors, nearest first.
	 * His FO4CSLOD is found through the mods folder, not the profile's stack, so the stack alone never has it. */
	if ( rel.startsWith( QLatin1String( "Data/" ), Qt::CaseInsensitive ) ) {
		const QString inData = rel.mid( 5 );
		QDir up( chunkDir );
		for ( int i = 0; i < 4 && up.cdUp(); i++ ) {
			const QString t = up.filePath( inData );
			if ( QFileInfo::exists( t ) )
				return QFileInfo( t ).absoluteFilePath();
		}
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
		// lane PERFLOAD1 P1: the MO2 mod "Fusion Shaders - Assets" (no plugin, so no stack names it) is a Data root
		// too: the impostor sets ship there, Data\FO4CSLOD\Cards\<id>_oct.lodm
		const QString assets = wwAssetsModRoot();
		if ( !assets.isEmpty() )
			roots << QDir::cleanPath( assets );
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
	// THE ENVIRONMENT WINS, and only for a measuring run.
	if ( wwEnvIsSet( "WW_IMPOSTOR_CHUNK" ) )
		return wwEnvInt( "WW_IMPOSTOR_CHUNK" ) != 0;
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
			QString path = resolveLodm( nif, s.chunkDir, p.c.lodm );
			// the C line names the model's own octahedral set AND the array atlas the same bake wrote with the
			// model's layer in it.
			WW_SWITCH_STATIC( bool, noAtlas, wwEnv( "WW_CELL_FARLOD_RED" ) == "noatlas" );
			bool viaAtlas = false;
			if ( path.isEmpty() && !p.c.arrayLodm.isEmpty() && !noAtlas ) {
				path = resolveLodm( nif, s.chunkDir, p.c.arrayLodm );
				viaAtlas = !path.isEmpty();
			}
			ImpostorCardSet set;
			if ( path.isEmpty() ) {
				set.ok = false;
				set.error = QStringLiteral( "not found beside the chunk or on the resource stack" );
				s.notes << QStringLiteral( "impostor chunk: %1 -- %2" ).arg( p.c.lodm, set.error );
			} else {
				set = impostorCardLoad( path, ( viaAtlas || !p.c.arrayLodm.isEmpty() ) ? p.c.layer : -1 );
				if ( !set.ok )
					s.notes << QStringLiteral( "impostor chunk: %1 -- %2" ).arg( path, set.error );
				else if ( viaAtlas )	// lane PERFLOAD1 P1: the atlas layer IS the octahedral set, say which kind it is
					s.notes << QStringLiteral( "impostor chunk: %1 not installed -- served by the bake's array atlas %2 "
						"layer %3, %4" ).arg( p.c.lodm, QFileInfo( path ).fileName() ).arg( p.c.layer )
						.arg( set.ring() ? QStringLiteral( "a horizon ring of %1 views" ).arg( set.views )
							: set.oct > 1 ? QStringLiteral( "the same octahedral impostor (%1 x %1 views)" ).arg( set.oct )
							: QStringLiteral( "a FLAT card (one view)" ) );
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

	// THE SHEETS FIRST, ALWAYS, AND ONCE.
	if ( !s.sheetsRegistered ) {
		s.sheetsRegistered = true;
		for ( const ImpostorCardSet & set : s.sets ) {
			if ( !set.ok )
				continue;
			const QString root = ImpostorDraw::registerLooseSheets( scene, set );
			if ( !root.isEmpty() )
				s.notes << QStringLiteral( "impostor chunk: registered %1" ).arg( root );
		}
		// AND THEN DRAW, in this same frame.
	}

	// THE COUNT, SAID ONCE, ON stdout.
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

/* ---- the cell view's far tree cards (impostorchunk.h) ---- */
int ImpostorChunk::armCellFar( NifModel * nif, const QStringList & chunkPaths, const float shift[3],
	float cx, float cy, float rMax, int hx0, int hy0, int hx1, int hy1,
	QSet<quint32> * refs, QStringList * notes, const QVector<QRect> * oursOnly )
{
	CellFarState & F = farSt();
	F = CellFarState();
	farGenRef()++;
	F.shift[0] = shift[0];
	F.shift[1] = shift[1];
	F.shift[2] = shift[2];

	// the chunk state is borrowed per manifest and put back as it was found
	const ChunkState saved = st();
	int read = 0, kept = 0, inHole = 0, beyond = 0, notOurs = 0;
	QSet<QString> said;   // a set's note once, not once a chunk
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
			/* lane PERFLOAD1 P3: with our files limited to a rectangle (WW_CELL_FARLOD_OURS) a card of ours stands only
			 * on a ring we built; the game's .bto carries its own trees everywhere else */
			if ( oursOnly ) {
				bool in = false;
				for ( const QRect & q : *oursOnly )
					in = in || q.contains( cellX, cellY );
				if ( !in ) {
					notOurs++;
					continue;
				}
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
				if ( ( n.contains( QLatin1String( "refused" ) ) || n.contains( QLatin1String( "not found" ) )
					|| n.contains( QLatin1String( "cannot" ) ) || n.contains( QLatin1String( "served by" ) ) )
					&& !said.contains( n ) ) {
					said.insert( n );
					*notes << n;
				}
	}
	st() = saved;
	if ( notes )
		*notes << QStringLiteral( "far lod: cards %1 read from %2 chunk manifests, %3 kept, %4 in the hole, "
			"%5 past %6 units, %7 sets" ).arg( read ).arg( chunkPaths.size() ).arg( kept ).arg( inHole )
			.arg( beyond ).arg( double( rMax ), 0, 'f', 0 ).arg( F.cards.sets.size() )
			+ ( oursOnly ? QStringLiteral( ", %1 outside our built rings (the game's .bto there)" ).arg( notOurs ) : QString() );
	return kept;
}

quint64 wwGlPaintCount();	// glview.cpp
#include "features/far-lod/lodpreview.h"	// lane PERFLOAD1 P8

int ImpostorChunk::drawCellFar( Scene * scene, const ImpostorDraw::Options & opt )
{
	CellFarState & F = farSt();
	ChunkState & s = F.cards;
	if ( !scene || s.placed.isEmpty() )
		return 0;
	/* lane PERFLOAD1 P7, THE TREE LADDER (treeladder.h, docs/FO4CS_IMPROVED_LOD_PLAN.md R3 4-5): every handed tree
	 * takes its step from its size on screen (bound radius x scale / distance x P11 / 2 of the live projection); a
	 * tree on its mesh step leaves its card out, a tree on the impostor hides its mesh row (CellFarHide), and every
	 * switch cross-fades (the card dithered per frame). Red WW_CELL_FARLOD_RED=edgecut keeps no mesh row: P1's cut. */
	static QElapsedTimer ladderClock, sinceCommit, sinceCast;
	const float dt = ladderClock.isValid() ? std::fmin( 0.25f, float( ladderClock.restart() ) / 1000.0f ) : 0.0f;
	if ( !ladderClock.isValid() )
		ladderClock.start();
	// the paint's number: the mesh rows' dither (CellFarInst) shifts its noise by the same frame
	const int fadeFrame = int( wwGlPaintCount() & 1023u );
	if ( F.ladderFor != farGen() || F.ladder.size() != s.placed.size() ) {
		F.ladderFor = farGen();
		F.ladder.fill( TreeLadder::State(), s.placed.size() );
		F.ladderHid.fill( 0, s.placed.size() );
		CellFarHide::ladderClear();
		sinceCommit.invalidate();
		sinceCast.invalidate();
	}
	CellFarHide::ladderFadesBegin( fadeFrame );
	const bool ladderPersp = scene->renderer && scene->renderer->globalUniforms->projectionMatrix[3][3] != 1.0f;
	const float p11 = ladderPersp ? scene->renderer->globalUniforms->projectionMatrix[1][1] : 0.0f;	// pm[column][row]
	static const TreeLadder::Thresholds ladderThr;
	int atStep[TreeLadder::Steps] = {};
	int ladderTrees = 0, ladderRows = 0, ladderFades = 0, ladderCards = 0, ladderCardsFading = 0;
	int ladderHeld = 0, ladderGoing = 0, ladderMeshCast = 0;	// round 2: fades held for a commit, rows dithering out
	const int forcedStep = LodPreview::force();	// lane PERFLOAD1 P8: the preview's level (-1 = the ladder)
	int ladderNearest = 0;
	if ( p11 > 0.0f ) {
		for ( int i = 0; i < s.placed.size(); i++ ) {
			const Placed & p = s.placed.at( i );
			if ( p.ladderRadius <= 0.0f )
				continue;
			/* round 2: every step the tree has rows for (one a distinct MNAM mesh), then the impostor; both sides of a
			 * switch dither (the rows by attribute 9, TreeLadder::rowFade), the casters take the target step */
			const uint8_t meshSteps = p.ladderRef ? uint8_t( p.ladderSteps & 0x1Eu ) : uint8_t( 0 );
			const bool row = meshSteps != 0;
			const uint8_t avail = uint8_t( ( 1u << TreeLadder::Impostor ) | meshSteps );
			const float sc = std::fabs( p.scale );
			const Vector3 v = scene->view * Vector3( p.world[0] - F.shift[0], p.world[1] - F.shift[1],
				p.world[2] - F.shift[2] + p.c.center[2] * sc );
			const float f = TreeLadder::screenFraction( p.ladderRadius * sc, v.length(), p11 );
			TreeLadder::State & st = F.ladder[i];
			int want = TreeLadder::select( f, avail, ladderThr, st.step );
			if ( forcedStep >= 0 ) {	// P8: the forced level, else the nearest step the tree has (counted)
				want = TreeLadder::forced( forcedStep, avail );
				if ( want != forcedStep )
					ladderNearest++;
			}
			TreeLadder::advance( st, want, dt );
			ladderTrees++;
			if ( st.step >= 0 && st.step < TreeLadder::Steps )
				atStep[st.step]++;
			if ( TreeLadder::fading( st ) )
				ladderFades++;
			if ( row ) {
				ladderRows++;
				const uint8_t draw = uint8_t( TreeLadder::drawMask( st ) & meshSteps );
				const uint8_t cast = uint8_t( ( 1u << st.step ) & meshSteps );
				if ( cast )
					ladderMeshCast++;
				const int how = CellFarHide::setLadder( p.ladderRef, draw, cast );
				if ( how == 1 && TreeLadder::fading( st ) ) {
					st.t = 0.0f;	// a row not live yet: the fade waits for the commit, so no side opens a hole
					ladderHeld++;
				}
				if ( how == 2 )
					ladderGoing++;
				if ( how == 2 || TreeLadder::fading( st ) )
					CellFarHide::ladderFade( p.ladderRef, st, draw );
			}
		}
	}
	// the mesh rows see the change at most every 100 ms (one row filter a bucket per commit); the first frame at once
	if ( CellFarHide::ladderDirty() && ( !sinceCommit.isValid() || sinceCommit.elapsed() >= 100 ) ) {
		CellFarHide::ladderCommit();
		sinceCommit.start();
	}
	// the casters (cascade rows + card pyramids) take the target step at most once a second: each commit rebuilds them
	if ( CellFarHide::castDirty() && ( !sinceCast.isValid() || sinceCast.elapsed() >= 1000 ) ) {
		CellFarHide::castCommit();
		sinceCast.start();
	}
	// lane PERFLOAD1 P8: the LOD preview's line on each change (the far trees by the step they go to)
	static quint64 saidPreview = 0;
	if ( ladderTrees > 0 && LodPreview::gen() != saidPreview ) {
		saidPreview = LodPreview::gen();
		qInfo().noquote() << QStringLiteral( "lod preview: level %1 (far trees per frame, far objects at the far rebuild), "
			"tint %2, ring %3, near grid %4; far trees per level full %5, rep0 %6, rep1 %7, rep2 %8, rep3 %9, impostor %10; "
			"%11 drawn at their nearest level (no %1 step)" )
			.arg( LodPreview::levelName( forcedStep ) ).arg( LodPreview::tint() ? "on" : "off" )
			.arg( LodPreview::ringName( LodPreview::ring() ) ).arg( LodPreview::gridName( LodPreview::grid() ) )
			.arg( atStep[0] ).arg( atStep[1] ).arg( atStep[2] ).arg( atStep[3] ).arg( atStep[4] ).arg( atStep[5] )
			.arg( ladderNearest );
	}
	const bool lodTint = LodPreview::tint();
	if ( !s.sheetsRegistered ) {
		s.sheetsRegistered = true;
		for ( const ImpostorCardSet & set : std::as_const( s.sets ) )
			if ( set.ok )
				ImpostorDraw::registerLooseSheets( scene, set );
	}
	static int saidDrawn = -1, saidOut = -1;
	int drawn = 0, outside = 0, refused = 0;
	QString firstWhy;	// a card the drawer refuses says why, once (was thrown away)
	// THE CAMERA'S CULL: a card whose bounding sphere is outside one of the four side planes is not drawn, the
	// game's per-object test (SUNCELL1's cellcull uses the same planes for the document's shapes).
	float pl[4][4];
	bool cull = wwEnv( "WW_CELL_FARLOD_RED" ) != "nocardcull" && scene->renderer
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
	// the culled cards go to one instanced draw per set; WW_FARCARD=0 / _RED=percard draw them one by one
	const bool inst = opt.cellLit && ImpostorDraw::instancedOn( scene );
	static QVector< ImpostorDraw::InstCard > instCards;
	instCards.clear();
	ImpostorDraw::batchBegin();	// the program, pass uniforms and sheets bound once, not per card
	for ( const Placed & p : std::as_const( s.placed ) ) {
		if ( p.setIndex < 0 || p.setIndex >= s.sets.size() )
			continue;
		if ( cardHidden( p ) )   // the near cell draws it
			continue;
		float fadeOut = 0.0f;	// lane PERFLOAD1 P7: the ladder's card weight
		if ( p.ladderRadius > 0.0f && p11 > 0.0f ) {
			const TreeLadder::State & st = F.ladder.at( int( &p - s.placed.constData() ) );
			const float w = st.step >= 0 ? TreeLadder::weight( st, TreeLadder::Impostor ) : 1.0f;
			if ( w <= 0.0f )
				continue;	// on its mesh step: the ring row draws it
			fadeOut = 1.0f - w;
		}
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
		float swayOff = 0.0f;
		if ( o.swayAmplitude != 0.0f ) {   // a forest does not sway as one
			swayOff = 0.0021f * p.world[0] + 0.0017f * p.world[1];
			o.swayPhase += swayOff;
		}
		if ( p.ladderRadius > 0.0f ) {
			ladderCards++;
			if ( fadeOut > 0.0f )
				ladderCardsFading++;
		}
		o.fadeOut = fadeOut;
		o.fadeFrame = float( fadeFrame );
		if ( lodTint )	// P8: the impostor's colour, a card mid-fade the band's
			LodPreview::tintColour( fadeOut > 0.0f ? 6 : int( TreeLadder::Impostor ), o.lodTint );
		if ( inst && s.sets.at( p.setIndex ).ok && fadeOut <= 0.0f ) {	// a fading card goes one by one (its own fade)
			ImpostorDraw::InstCard c;
			c.set = &s.sets.at( p.setIndex );
			c.offset[0] = p.world[0] - F.shift[0];
			c.offset[1] = p.world[1] - F.shift[1];
			c.offset[2] = p.world[2] - F.shift[2];
			c.scale = p.scale;
			c.swayPhase = o.swayPhase;
			c.slot = int( &p - s.placed.constData() );	// lane FARCARDPICK1: the GPU pick's static slot
			c.swayOff = swayOff;
			instCards << c;
			continue;
		}
		QString why;
		if ( ImpostorDraw::drawCard( scene, s.sets.at( p.setIndex ),
				Vector3( p.world[0] - F.shift[0], p.world[1] - F.shift[1], p.world[2] - F.shift[2] ), o, &why ) )
			drawn++;
		else if ( refused++ == 0 )
			firstWhy = QFileInfo( s.sets.at( p.setIndex ).lodmPath ).fileName() + QStringLiteral( ": " ) + why;
	}
	ImpostorDraw::batchEnd( scene );
	ImpostorDraw::InstStats ist;
	if ( !instCards.isEmpty() ) {
		ImpostorDraw::Options io = opt;
		if ( lodTint )	// P8
			LodPreview::tintColour( TreeLadder::Impostor, io.lodTint );
		if ( ImpostorDraw::drawCardsInstanced( scene, instCards, io, farGen(), &ist ) ) {
			drawn += ist.drawn;
			if ( ist.refused && !refused )
				firstWhy = ist.firstWhy;
			refused += ist.refused;
		} else {	// the instanced program is missing: the cards the old way, so nothing vanishes
			ImpostorDraw::batchBegin();
			for ( const ImpostorDraw::InstCard & c : std::as_const( instCards ) ) {
				ImpostorDraw::Options o = opt;
				o.worldScale = c.scale;
				o.swayPhase = c.swayPhase;
				QString why;
				if ( ImpostorDraw::drawCard( scene, *c.set, Vector3( c.offset[0], c.offset[1], c.offset[2] ), o, &why ) )
					drawn++;
				else if ( refused++ == 0 )
					firstWhy = QFileInfo( c.set->lodmPath ).fileName() + QStringLiteral( ": " ) + why;
			}
			ImpostorDraw::batchEnd( scene );
			ist.sets = -1;
		}
	}
	static int saidSets = -2, saidDraws = -1, sinceSaid = 0;
	// once more 60 frames after a change: the GPU pick's first frame sends the whole static buffer
	const bool steady = inst && ++sinceSaid == 60;
	if ( inst && wwFrameLog() && ( ist.sets != saidSets || ist.draws != saidDraws || steady ) ) {
		if ( !steady )
			sinceSaid = 0;
		saidSets = ist.sets;
		saidDraws = ist.draws;
		qInfo().noquote() << ( ist.sets < 0
			? QStringLiteral( "far cards: instanced program missing, drawn one by one" )
			: QStringLiteral( "far cards: instanced, %1 cards in %2 draws (%3 sets), %4 KB a frame (%5 pick%6)" )
				.arg( ist.drawn ).arg( ist.draws ).arg( ist.sets ).arg( double( ist.bytes ) / 1024.0, 0, 'f', 1 )
				.arg( ist.gpuPick ? QStringLiteral( "GPU" ) : QStringLiteral( "CPU" ) )
				.arg( steady ? QStringLiteral( ", steady" ) : QString() ) );
	}
	// lane PERFLOAD1 P7: the ladder's census, once a second at most while it changes
	static QString saidLadder;
	static QElapsedTimer sinceLadderSaid;
	if ( ladderTrees > 0 && wwFrameLog() && ( !sinceLadderSaid.isValid() || sinceLadderSaid.elapsed() >= 1000 ) ) {
		const QString line = QStringLiteral( "far trees ladder: of %1 trees (%2 with a mesh row) full %3, rep0 %4, rep1 %5, "
			"rep2 %6, rep3 %7, impostor %8; cards drawn %9 (%10 fading), cross-fades in flight %11, mesh rows hidden %12; "
			"thresholds %13 of the screen height (x%14 back), fade %15 s, P11 %16 (live projection, fovY %17 deg)%18" )
			.arg( ladderTrees ).arg( ladderRows ).arg( atStep[0] ).arg( atStep[1] ).arg( atStep[2] ).arg( atStep[3] )
			.arg( atStep[4] ).arg( atStep[5] ).arg( ladderCards ).arg( ladderCardsFading ).arg( ladderFades )
			.arg( CellFarHide::ladderHiddenCount() )
			.arg( QStringLiteral( "%1/%2/%3/%4/%5" ).arg( ladderThr.thr[0] ).arg( ladderThr.thr[1] ).arg( ladderThr.thr[2] )
				.arg( ladderThr.thr[3] ).arg( ladderThr.thr[4] ) )
			.arg( TreeLadder::kHysteresis ).arg( TreeLadder::kFadeSeconds ).arg( p11, 0, 'f', 3 )
			.arg( p11 > 0.0f ? 2.0 * std::atan( 1.0 / p11 ) * 57.29577951 : 0.0, 0, 'f', 1 )
			.arg( wwEnv( "WW_CELL_FARLOD_RED" ) == "edgecut" ? QStringLiteral( " (RED edgecut)" ) : QString() )
			+ QStringLiteral( "; round 2: rows dithered %1 (since the last census), fades held for a commit %2, rows "
				"dithering out %3, mesh casters %4 of %5 (card casters the rest, commits <= 1/s)" )
				.arg( CellFarHide::ladderDitherRows() ).arg( ladderHeld ).arg( ladderGoing ).arg( ladderMeshCast )
				.arg( ladderRows );
		CellFarHide::ladderDitherRows() = 0;
		if ( line != saidLadder ) {
			saidLadder = line;
			sinceLadderSaid.start();
			qInfo().noquote() << line;
		}
	}
	if ( ( drawn != saidDrawn || outside != saidOut ) && wwFrameLog() ) {
		saidDrawn = drawn;
		saidOut = outside;
		qInfo().noquote() << QStringLiteral( "far lod: drew %1 cards of %2 placed, %3 outside the camera%4%5" )
			.arg( drawn ).arg( s.placed.size() ).arg( outside )
			.arg( cull ? QString() : QStringLiteral( " (cull off%1)" )
				.arg( wwEnv( "WW_CELL_FARLOD_RED" ) == "nocardcull" ? QStringLiteral( ", RED nocardcull" ) : QString() ) )
			.arg( refused ? QStringLiteral( ", %1 refused by the drawer (first: %2)" ).arg( refused ).arg( firstWhy ) : QString() );
	}
	return drawn;
}

int ImpostorChunk::cellFarTreeForms( const QString & objectsDir, const QString & ws, QHash<quint32, int> * layerOf,
	QStringList * notes )
{
	CellFarState & F = farSt();
	F.treeSet.clear();
	if ( layerOf )
		layerOf->clear();
	const QStringList files = QDir( objectsDir ).entryList( { ws + QStringLiteral( ".LodgenCards.*.lodm" ) },
		QDir::Files, QDir::Name | QDir::IgnoreCase );
	int arrays = 0, ringFiles = 0, dup = 0, refused = 0, flat = 0, oct = 0;
	QString firstRefusal;
	for ( const QString & fn : files ) {
		if ( fn.endsWith( QLatin1String( ".ring.lodm" ), Qt::CaseInsensitive ) ) {
			ringFiles++;	// horizon ring sets: not the octahedral impostor of a form
			continue;
		}
		const QString path = QDir( objectsDir ).filePath( fn );
		QFile f( path );
		if ( !f.open( QIODevice::ReadOnly ) )
			continue;
		const LodmMaterial m = lodmParse( f.readAll() );
		if ( !m.ok || m.kind != QLatin1String( "cardArray" ) )
			continue;
		arrays++;
		const QJsonArray layers = m.root.value( QLatin1String( "array" ) ).toObject().value( QLatin1String( "layers" ) ).toArray();
		for ( int i = 0; i < layers.size(); i++ ) {
			bool ok = false;
			const quint32 form = layers.at( i ).toObject().value( QLatin1String( "id" ) ).toString().toUInt( &ok, 16 );
			if ( !ok )
				continue;
			if ( F.treeSet.contains( form ) ) {
				dup++;	// an older array naming the same form: the first file by name wins
				continue;
			}
			const ImpostorCardSet set = impostorCardLoad( path, i );
			if ( !set.ok || set.ring() ) {
				if ( refused++ == 0 )
					firstRefusal = set.ok ? QStringLiteral( "%1 layer %2: a horizon ring" ).arg( fn ).arg( i ) : set.error;
				continue;
			}
			( set.oct > 1 ? oct : flat )++;
			F.cards.sets << set;
			F.cards.lodmKeys << path.toLower() + QStringLiteral( "#" ) + QString::number( i );
			F.treeSet.insert( form, F.cards.sets.size() - 1 );
			if ( layerOf )
				layerOf->insert( form, i );
		}
	}
	if ( notes )
		*notes << QStringLiteral( "far lod: tree impostors: %1 forms from %2 card arrays in %3 (%4 octahedral, %5 flat; "
			"%6 ring files left out, %7 duplicate forms, %8 refused%9)" ).arg( F.treeSet.size() ).arg( arrays )
			.arg( QDir::toNativeSeparators( objectsDir ) ).arg( oct ).arg( flat ).arg( ringFiles ).arg( dup ).arg( refused )
			.arg( refused ? QStringLiteral( ", first: " ) + firstRefusal : QString() );
	return F.treeSet.size();
}

int ImpostorChunk::addCellFarTrees( const QVector<LodiTreeCard> & trees )
{
	CellFarState & F = farSt();
	int added = 0;
	for ( const LodiTreeCard & t : trees ) {
		const auto it = F.treeSet.constFind( t.form );
		if ( it == F.treeSet.constEnd() )
			continue;
		const ImpostorCardSet & set = F.cards.sets.at( it.value() );
		Placed p;
		p.c.ok = true;
		p.c.index = -1;
		p.c.center[0] = set.center[0];
		p.c.center[1] = set.center[1];
		p.c.center[2] = set.center[2];
		p.c.halfW = set.halfW;
		p.c.halfH = set.halfH;
		p.c.oct = set.oct;
		p.c.depthSpan = set.depthSpan;
		p.c.lodm = set.lodmPath;
		p.c.arrayLodm = set.lodmPath;
		p.c.layer = set.layer;
		p.world[0] = t.xyz[0];
		p.world[1] = t.xyz[1];
		p.world[2] = t.xyz[2];
		p.scale = t.scale > 0.0f ? t.scale : 1.0f;
		p.formId = QString::number( t.form, 16 ).rightJustified( 8, QLatin1Char( '0' ) );
		if ( t.ref )
			p.ref = QString::number( t.ref, 16 ).rightJustified( 8, QLatin1Char( '0' ) );
		p.setIndex = it.value();
		// lane PERFLOAD1 P7: the ladder's inputs (no .lodo radius: the card's own half diagonal)
		p.ladderRadius = t.radius > 0.0f ? t.radius : std::sqrt( set.halfW * set.halfW + set.halfH * set.halfH );
		p.ladderMeshStep = qint8( t.meshStep );
		p.ladderRef = t.meshStep >= 0 ? t.ref : 0u;
		p.ladderSteps = t.meshStep >= 0 ? quint8( t.stepMask | ( 1u << t.meshStep ) ) : quint8( 0 );
		F.cards.placed << p;
		added++;
	}
	if ( added )
		farGenRef()++;
	return added;
}

void ImpostorChunk::cellFarCensus( int * octahedral, int * ring, int * flat )
{
	const ChunkState & s = farSt().cards;
	int o = 0, r = 0, f = 0;
	for ( const Placed & p : s.placed ) {
		if ( p.setIndex < 0 || p.setIndex >= s.sets.size() )
			continue;
		const ImpostorCardSet & set = s.sets.at( p.setIndex );
		if ( set.ring() )
			r++;
		else if ( set.oct > 1 )
			o++;
		else
			f++;
	}
	if ( octahedral )
		*octahedral = o;
	if ( ring )
		*ring = r;
	if ( flat )
		*flat = f;
}

void ImpostorChunk::forgetCellFar()
{
	farSt() = CellFarState();
	farGenRef()++;
	CellFarHide::ladderClear();	// lane PERFLOAD1 P7: no mesh row stays hidden for a tree gone
}

int ImpostorChunk::cellFarCount()
{
	return farSt().cards.placed.size();
}

quint64 ImpostorChunk::cellFarGen()
{
	// lane PERFLOAD1 P7 round 2: a ladder cast commit moves the card casters too
	return farGen() + ( ( cardHide().gen + CellFarHide::ladderCastGen() ) << 32 );
}

void ImpostorChunk::cellFarHideSet( const QSet<qint64> & cells )
{
	CardHide & h = cardHide();
	if ( h.cells == cells )
		return;
	h.cells = cells;
	h.gen++;
}

int ImpostorChunk::cellFarCardsShownIn( const QSet<qint64> & cells )
{
	int n = 0;
	for ( const Placed & p : farSt().cards.placed )
		if ( cells.contains( cardCell( p ) ) && !cardHidden( p ) )
			n++;
	return n;
}

int ImpostorChunk::cellFarCasters( QVector<FarCasterPiece> & out, float apexScale, const float * keep, int * cut )
{
	out.clear();
	if ( cut )
		*cut = 0;
	const CellFarState & F = farSt();
	const ChunkState & s = F.cards;
	int used = 0, curIn = -1, curOut = -1;
	for ( const Placed & p : s.placed ) {
		if ( p.setIndex < 0 || p.setIndex >= s.sets.size() || !s.sets.at( p.setIndex ).ok )
			continue;
		if ( cardHidden( p ) )
			continue;
		// lane PERFLOAD1 P7 round 2: a ladder tree on a mesh step casts with its mesh row (CellFarHide view( d, true ))
		if ( p.ladderRef && p.ladderSteps && ( CellFarHide::ladderCast( p.ladderRef ) & p.ladderSteps )
			&& CellFarHide::ladderCast( p.ladderRef ) != 0xFF )
			continue;
		const float sc = std::fabs( p.scale );
		const float cx = p.world[0] - F.shift[0] + p.c.center[0] * sc, cy = p.world[1] - F.shift[1] + p.c.center[1] * sc;
		const float cz = p.world[2] - F.shift[2] + p.c.center[2] * sc;
		const float w = p.c.halfW * sc, h = p.c.halfH * sc;
		const bool nearOut = keep && ( cx - w < keep[0] || cy - w < keep[1] || cx + w > keep[2] || cy + w > keep[3] );
		if ( nearOut && cut )
			++*cut;
		int & cur = nearOut ? curOut : curIn;	// SHAFTS1b: one piece holds pyramids of one side of the near window only
		if ( cur < 0 || out.at( cur ).verts.size() >= 13000 * 5 ) {
			cur = int( out.size() );
			out.append( FarCasterPiece() );
			out.last().nearOut = nearOut;
		}
		FarCasterPiece & k = out[cur];
		const int b = int( k.verts.size() );
		k.verts << Vector3( cx, cy, cz + h * apexScale ) << Vector3( cx - w, cy - w, cz ) << Vector3( cx + w, cy - w, cz )
			<< Vector3( cx + w, cy + w, cz ) << Vector3( cx - w, cy + w, cz );
		for ( int f = 0; f < 4; f++ )
			k.tris << Triangle( quint16( b ), quint16( b + 1 + f ), quint16( b + 1 + ( f + 1 ) % 4 ) );
		used++;
	}
	return used;
}
