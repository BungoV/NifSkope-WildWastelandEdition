/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#include "cellfarlod.h"

#include "btdterrain.h"
#include "cellfarvanilla.h"
#include "esmdata.h"
#include "esmwater.h"
#include "impostorchunk.h"
#include "cellmesh.h"
#include "lodinative.h"
#include "lodgen.h"
#include "gamemanager.h"
#include "gl/cellhdr.h"
#if __has_include( "gl/cellcull.h" )
#include "gl/cellcull.h"   // lane SUNCELL1's culling, when merged
#define FARLOD1_HAVE_CULL 1
#endif
#include "gl/cellwater.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

// lane FARLOD1: see cellfarlod.h

namespace
{

constexpr float kCell = 4096.0f;
/* lane FARLOD2: what one far-object triangle costs when its ring keeps its arrays beside the document
 * (src/cellmesh.h) instead of as rows. Rows were 2.4 KB a triangle (FINALFIX g3). MEASURED: see the
 * "B a triangle measured" telemetry and notes/finalfix/farlod2.md; the largest ring's figure, rounded up. */
constexpr double kSideBytesPerTri = 560.0;	// measured 10-06 a1: 545 / 238 / 498 / 81 B rings 0-3 (Sanctuary), largest rounded up

//! What one cell document's far field is, for the draw (keyed by the document).
struct FarDoc
{
	float reach = 0.0f;           //!< scene units from the scene origin to the farthest built ring corner
	float origin[3] = { 0.0f, 0.0f, 0.0f };
	int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1;
	int cards = 0;
	int fx0 = 0, fy0 = 0, fx1 = -1, fy1 = -1;   //!< the outermost built ring's footprint, cells (the far field's edge)
};

QHash<const void *, FarDoc> & docs()
{
	static QHash<const void *, FarDoc> d;
	return d;
}

QSet<const void *> & hooked()
{
	static QSet<const void *> h;
	return h;
}

void memNow( double & wsMB, double & peakMB )
{
	wsMB = peakMB = 0.0;
#ifdef Q_OS_WIN
	PROCESS_MEMORY_COUNTERS pmc;
	if ( GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) ) ) {
		wsMB = double( pmc.WorkingSetSize ) / ( 1024.0 * 1024.0 );
		peakMB = double( pmc.PeakWorkingSetSize ) / ( 1024.0 * 1024.0 );
	}
#endif
}

int envInt( const char * name, int fallback )
{
	bool ok = false;
	const int v = qEnvironmentVariableIntValue( name, &ok );
	return ok ? v : fallback;
}

float envFloat( const char * name, float fallback )
{
	bool ok = false;
	const float v = qEnvironmentVariable( name ).toFloat( &ok );
	return ok ? v : fallback;
}

int floorDiv( int a, int b )
{
	return ( a >= 0 ) ? a / b : -( ( -a + b - 1 ) / b );
}

//! The far sway's amplitude in units (0 = off): WW_CELL_FARLOD_SWAY, default 10.
float swayAmp()
{
	return std::max( 0.0f, envFloat( "WW_CELL_FARLOD_SWAY", 10.0f ) );
}

//! Seconds for the sway: WW_CELL_FARLOD_SWAYTIME pins it (a still picture), else the session clock.
float swayTime()
{
	bool ok = false;
	const float pinned = qEnvironmentVariable( "WW_CELL_FARLOD_SWAYTIME" ).toFloat( &ok );
	if ( ok )
		return pinned;
	static QElapsedTimer clock;
	if ( !clock.isValid() )
		clock.start();
	return float( double( clock.elapsed() ) / 1000.0 );
}

const FarDoc * farFor( const Scene * scene )
{
	if ( !scene || !scene->nifModel )
		return nullptr;
	auto it = docs().constFind( scene->nifModel );
	return it == docs().constEnd() ? nullptr : &*it;
}

} // namespace

bool cellFarLodWanted()
{
	// the environment wins, and only for a measuring run (a harness never writes his settings)
	if ( qEnvironmentVariableIsSet( "WW_CELL_FARLOD" ) )
		return qEnvironmentVariableIntValue( "WW_CELL_FARLOD" ) != 0;
	return QSettings().value( QStringLiteral( "CellView/FarLod" ), false ).toBool();
}

void cellFarLodSetWanted( bool on )
{
	QSettings().setValue( QStringLiteral( "CellView/FarLod" ), on );
}

namespace
{
int & farlod1LastType()
{
	static int t = WwFarLodFo4cs;
	return t;
}

//! The type this build takes, and why (env pin, his setting, or auto on whether our files exist).
int farlod1TypeFor( bool oursFound, QString * why )
{
	const QByteArray env = qgetenv( "WW_CELL_FARLOD_TYPE" ).toLower();
	if ( env == "vanilla" || env == "fo4cs" ) {
		*why = QStringLiteral( "WW_CELL_FARLOD_TYPE" );
		return env == "vanilla" ? WwFarLodVanilla : WwFarLodFo4cs;
	}
	const int pinned = cellFarLodTypePinned();
	if ( pinned != WwFarLodAuto ) {
		*why = QStringLiteral( "setting" );
		return pinned;
	}
	*why = oursFound ? QStringLiteral( "auto, our files found" ) : QStringLiteral( "auto, no FO4CSLOD files" );
	return oursFound ? WwFarLodFo4cs : WwFarLodVanilla;
}

/* THE FAR SHAPES IN SUNCELL1'S CULL TABLE. Every far shape (ours and the game's) is one big weld, so a
 * whole-shape test culls nothing: cut each into runs of <= 256 consecutive triangles, a new run as soon as a
 * triangle's centre lies over 2048 units from the run's first, each with a bounding sphere in the shape's own
 * space. The camera pass then skips the runs outside the frustum; the cascades cull their casters themselves. */
QString farlod1CullRegister( NifModel * nif, int firstBlock )
{
#ifdef FARLOD1_HAVE_CULL
	int shapes = 0, runsAll = 0;
	for ( int b = firstBlock + 1; b < nif->getBlockCount(); b++ ) {
		const QModelIndex iB = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iB, "BSTriShape" ) )
			continue;
		if ( !nif->get<QString>( iB, "Name" ).startsWith( QLatin1String( "FarLOD" ) ) )
			continue;
		if ( nif->get<quint32>( iB, "Flags" ) & 1u )
			continue;
		/* lane FARLOD2: a far object shape keeps its arrays beside the document (src/cellmesh.h); read them there.
		 * Looking its rows up by name would WRITE them (the cellmesh net) and undo the representation. */
		int nv = 0;
		std::vector<Vector3> p;
		QVector<Triangle> tris;
		if ( const QSharedPointer<CellMesh> cm = cellMeshFor( nif, iB ) ) {
			nv = cm->verts.size();
			p.assign( cm->verts.cbegin(), cm->verts.cend() );
			tris = cm->triangles;
		} else {
			const QModelIndex iVD = nif->getIndex( iB, "Vertex Data" );
			const QModelIndex iTri = nif->getIndex( iB, "Triangles" );
			if ( !iVD.isValid() || !iTri.isValid() )
				continue;
			const bool fullPrec = ( ( nif->get<BSVertexDesc>( iB, "Vertex Desc" ).Value() >> 44 ) & VF_FULLPREC ) != 0;
			nv = nif->rowCount( iVD );
			p.resize( size_t( std::max( nv, 0 ) ) );
			for ( int v = 0; v < nv; v++ )
				p[size_t( v )] = fullPrec ? nif->get<Vector3>( nif->index( v, 0, iVD ), "Vertex" )
					: Vector3( nif->get<HalfVector3>( nif->index( v, 0, iVD ), "Vertex" ) );
			tris = nif->getArray<Triangle>( iTri );
		}
		std::vector<WwCullRun> runs;
		Vector3 first, lo, hi;
		std::vector<std::uint32_t> member;   // the triangles of the open run, for its radius
		auto close = [&]() {
			if ( member.empty() )
				return;
			WwCullRun r;
			r.first = member.front();
			r.count = std::uint32_t( member.size() );
			r.center = ( lo + hi ) * 0.5f;
			float rad = 0.0f;
			for ( std::uint32_t t : member ) {
				const Triangle & tr = tris[int( t )];
				for ( int k = 0; k < 3; k++ ) {
					const int vi = k == 0 ? tr.v1() : ( k == 1 ? tr.v2() : tr.v3() );
					if ( vi < nv )
						rad = std::max( rad, ( p[size_t( vi )] - r.center ).length() );
				}
			}
			r.radius = rad;
			runs.push_back( r );
			member.clear();
		};
		for ( int t = 0; t < tris.size(); t++ ) {
			const Triangle & tr = tris[t];
			const bool ok = tr.v1() < nv && tr.v2() < nv && tr.v3() < nv;
			const Vector3 c = ok ? ( p[tr.v1()] + p[tr.v2()] + p[tr.v3()] ) / 3.0f : Vector3();
			if ( !member.empty() && ( member.size() >= 256 || ( ok && ( c - first ).length() > 2048.0f ) ) )
				close();
			if ( member.empty() ) {
				first = c;
				lo = hi = c;
			}
			member.push_back( std::uint32_t( t ) );
			if ( ok ) {
				for ( const Vector3 & q : { p[tr.v1()], p[tr.v2()], p[tr.v3()] } ) {
					for ( int a = 0; a < 3; a++ ) {
						lo[a] = std::min( lo[a], q[a] );
						hi[a] = std::max( hi[a], q[a] );
					}
				}
			}
		}
		close();
		if ( runs.size() < 2 )
			continue;   // one run: nothing to cull inside the shape
		shapes++;
		runsAll += int( runs.size() );
		wwCellCullShape( nif, b, std::move( runs ) );
	}
	return QStringLiteral( "far lod: cull runs %1 over %2 shapes (camera cull + cascade casters, SUNCELL1)" )
		.arg( runsAll ).arg( shapes );
#else
	Q_UNUSED( nif );
	Q_UNUSED( firstBlock );
	return QStringLiteral( "far lod: cull not registered (no SUNCELL1 culling in this build)" );
#endif
}
} // namespace

int cellFarLodTypePinned()
{
	const QString v = QSettings().value( QStringLiteral( "CellView/FarLodType" ) ).toString().toLower();
	return v == QLatin1String( "vanilla" ) ? WwFarLodVanilla : v == QLatin1String( "fo4cs" ) ? WwFarLodFo4cs : WwFarLodAuto;
}

void cellFarLodSetType( int type )
{
	QSettings cfg;
	if ( type == WwFarLodVanilla || type == WwFarLodFo4cs )
		cfg.setValue( QStringLiteral( "CellView/FarLodType" ),
			type == WwFarLodVanilla ? QStringLiteral( "vanilla" ) : QStringLiteral( "fo4cs" ) );
	else
		cfg.remove( QStringLiteral( "CellView/FarLodType" ) );
}

int cellFarLodLastType()
{
	return farlod1LastType();
}

QString cellFarLodAppend( NifModel * nif, const QModelIndex & iRoot, const EsmWorld & world,
	const QString & ws, int cx, int cy, int n, const QString & lodlPath, const Vector3 & origin )
{
	QStringList L;
	auto say = [&L]( const QString & line ) {
		L << line;
		qInfo().noquote() << line;
	};
	QElapsedTimer clock;
	clock.start();
	docs().remove( nif );
	ImpostorChunk::forgetCellFar();

	// the gates' red controls: nocut | nosnap | watercut | nowater (cardsafter, nocardcull, pbrnosway act at draw)
	const QByteArray red = qgetenv( "WW_CELL_FARLOD_RED" );
	const bool noCut = red == "nocut";
	const bool noSnap = red == "nosnap";
	const bool waterCut = red == "watercut";
	const bool noWater = red == "nowater";
	const bool vanBoth = red == "vanboth";   // the vanilla fill drawn over our chunks too (the mixed gate's red)
	const int h = std::max( 0, ( n - 1 ) / 2 );
	const int bx0 = cx - h, by0 = cy - h, bx1 = cx + h, by1 = cy + h;

	/* THE NEAR GATE'S ARM (WW_CELL_FARLOD_NEARONLY=1): the far field's planes and nothing else -- no shape,
	 * no card -- so a picture against the off path shows exactly what the planes alone change. */
	if ( qEnvironmentVariableIntValue( "WW_CELL_FARLOD_NEARONLY" ) != 0 && nif ) {
		FarDoc fd;
		fd.origin[0] = origin[0];
		fd.origin[1] = origin[1];
		fd.origin[2] = origin[2];
		fd.bx0 = bx0;
		fd.by0 = by0;
		fd.bx1 = bx1;
		fd.by1 = by1;
		fd.reach = envFloat( "WW_CELL_FARLOD_MAX", 250000.0f );
		docs().insert( nif, fd );
		say( QStringLiteral( "far lod: NEARONLY -- the planes alone, reach %1 units, near %2" )
			.arg( double( fd.reach ), 0, 'f', 0 ).arg( wwCellFarLodNear( 0.0 ), 0, 'f', 2 ) );
		return L.join( QLatin1Char( '\n' ) ) + QLatin1Char( '\n' );
	}

	/* THE TYPE: FO4CS (ours, the game's chunks filling any we lack) or Vanilla (the game's alone) */
	QString typeWhy;
	const int type = farlod1TypeFor( !lodlPath.isEmpty(), &typeWhy );
	farlod1LastType() = type;
	const bool ours = type == WwFarLodFo4cs && !lodlPath.isEmpty();
	const QString typeName = type == WwFarLodVanilla ? QStringLiteral( "vanilla" ) : QStringLiteral( "fo4cs" );
	say( QStringLiteral( "far lod: source type %1 (%2): %3" ).arg( typeName ).arg( typeWhy )
		.arg( lodlPath.isEmpty() ? QStringLiteral( "no FO4CSLOD/%1/%1.lodl found, the game's LOD everywhere" ).arg( ws )
			: ours ? QStringLiteral( "ours from %1, the game's where we have no chunk" ).arg( QDir::toNativeSeparators( lodlPath ) )
				: QStringLiteral( "our files found and NOT used" ) ) );
	if ( !nif || !iRoot.isValid() ) {
		say( QStringLiteral( "far lod: NONE -- no cell document" ) );
		return L.join( QLatin1Char( '\n' ) ) + QLatin1Char( '\n' );
	}

	/* THE GAME'S DISTANCES (his Fallout4Prefs.ini): fBlockLevel0Distance 60000, Level1 90000, Level2 110000,
	 * fBlockMaximumDistance 250000; fTreeLoadDistance 75000. Each ring reaches its level's distance in
	 * cells from the block centre; the env names are for a measuring run. */
	const float d0 = envFloat( "WW_CELL_FARLOD_L0", 60000.0f );
	const float d1 = envFloat( "WW_CELL_FARLOD_L1", 90000.0f );
	const float d2 = envFloat( "WW_CELL_FARLOD_L2", 110000.0f );
	const float dMax = envFloat( "WW_CELL_FARLOD_MAX", 250000.0f );
	/* lane FARLOD2: NO tree cut by default. fTreeLoadDistance is the FULL trees' reach; the game's tree LOD
	 * lives inside the .bto and is drawn with the object LOD out to fBlockMaximumDistance (FARLOD1 measured:
	 * FO4 has no .btt). The 75000 cut dropped 2558 cards at the FARLOD1 eye. WW_CELL_FARLOD_TREE_DIST=<units>
	 * puts a cut back (the gate's red control and a measuring run); 0 = none. */
	const float treeDist = std::max( 0.0f, envFloat( "WW_CELL_FARLOD_TREE_DIST", 0.0f ) );
	const QString treeCut = treeDist > 0.0f ? QStringLiteral( "past %1" ).arg( double( treeDist ), 0, 'f', 0 )
		: QStringLiteral( "cut (none: the game's reach)" );
	auto cellsTo = []( float d ) { return int( std::ceil( d / kCell ) ); };

	struct RingDef { int lod, sheetDim, reach, slot; };
	std::vector<RingDef> defs = {
		/* the band at the block: 32 samples a cell, the LAND's own 128-unit lattice. FINALFIX: at 16 a cell the
		 * ring's edge already sat on the LAND at its vertices, but the LAND has a vertex between each pair, so the
		 * seam stood open 140 units (measured, snap on and off alike). The RED nosnap keeps the old 16 a cell. */
		{ noSnap ? 1 : 0, 8, std::max( h + 1, 3 ), 0 },
		{ 2, 8, std::max( h + 2, cellsTo( d0 ) ), 0 },   // LOD4 objects
		{ 3, 16, cellsTo( d1 ), 1 },                     // LOD8
		{ 4, 16, cellsTo( d2 ), 2 },                     // LOD16
		{ 5, 32, cellsTo( dMax ), 3 },                   // LOD32
	};
	const int ringsWanted = qBound( 1, envInt( "WW_CELL_FARLOD_RINGS", int( defs.size() ) ), int( defs.size() ) );
	defs.resize( size_t( ringsWanted ) );
	for ( size_t i = 1; i < defs.size(); i++ )
		defs[i].reach = std::max( defs[i].reach, defs[i - 1].reach + 1 );

	// the budget: his 12 GB guard killed FARVIEW1. The far field's own share, default 2 GB.
	const double budgetMB = std::max( 64, envInt( "WW_CELL_FARLOD_BUDGET_MB", 2048 ) );
	constexpr double kBytesPerVert = 1536.0;   // a BSTriShape vertex row in the document + its GL copy, measured order
	const qint64 maxVerts = qint64( budgetMB * 1024.0 * 1024.0 / kBytesPerVert );

	double ws0 = 0, pk0 = 0;
	memNow( ws0, pk0 );

	// the node every far shape hangs from: its name keeps it out of the scene bounds (glscene.cpp)
	const QModelIndex iFar = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	nif->set<QString>( iFar, "Name", QStringLiteral( "FarLOD" ) );
	nif->set<quint32>( iFar, "Flags", 14 );
	nif->set<float>( iFar, "Scale", 1.0f );
	addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iFar ) );
	const int firstFarBlock = nif->getBlockNumber( iFar );

	// ---- the loaded LAND, for ring 0's inner edge (bilinear in a cell: exact along a cell edge)
	auto lands = std::make_shared<std::map<std::pair<int, int>, std::shared_ptr<EsmLand>>>();
	const EsmWorld * W = &world;
	auto innerZ = [lands, W, bx0, by0, bx1, by1]( float wx, float wy, float * z ) -> bool {
		const int lx = qBound( bx0, int( std::floor( wx / kCell ) ), bx1 );
		const int ly = qBound( by0, int( std::floor( wy / kCell ) ), by1 );
		auto key = std::make_pair( lx, ly );
		auto it = lands->find( key );
		if ( it == lands->end() ) {
			auto land = std::make_shared<EsmLand>();
			if ( !W->land( lx, ly, *land ) || !land->valid )
				land.reset();
			it = lands->emplace( key, land ).first;
		}
		if ( !it->second )
			return false;
		const EsmLand & Ld = *it->second;
		const float fx = qBound( 0.0f, ( wx - float( lx ) * kCell ) / 128.0f, 32.0f );
		const float fy = qBound( 0.0f, ( wy - float( ly ) * kCell ) / 128.0f, 32.0f );
		const int i0 = std::min( int( fx ), 31 ), j0 = std::min( int( fy ), 31 );
		const float tx = fx - float( i0 ), ty = fy - float( j0 );
		const float a = Ld.heights[j0][i0] * ( 1.0f - tx ) + Ld.heights[j0][i0 + 1] * tx;
		const float b = Ld.heights[j0 + 1][i0] * ( 1.0f - tx ) + Ld.heights[j0 + 1][i0 + 1] * tx;
		*z = a * ( 1.0f - ty ) + b * ty;
		return true;
	};

	// the LAND anywhere: the vanilla fill's seam (its clip lines stand on it)
	auto landZ = [lands, W]( float wx, float wy, float * z ) -> bool {
		const int lx = int( std::floor( wx / kCell ) ), ly = int( std::floor( wy / kCell ) );
		auto key = std::make_pair( lx, ly );
		auto it = lands->find( key );
		if ( it == lands->end() ) {
			auto land = std::make_shared<EsmLand>();
			if ( !W->land( lx, ly, *land ) || !land->valid )
				land.reset();
			it = lands->emplace( key, land ).first;
		}
		if ( !it->second )
			return false;
		const EsmLand & Ld = *it->second;
		const float fx = qBound( 0.0f, ( wx - float( lx ) * kCell ) / 128.0f, 32.0f );
		const float fy = qBound( 0.0f, ( wy - float( ly ) * kCell ) / 128.0f, 32.0f );
		const int i0 = std::min( int( fx ), 31 ), j0 = std::min( int( fy ), 31 );
		const float tx = fx - float( i0 ), ty = fy - float( j0 );
		const float a = Ld.heights[j0][i0] * ( 1.0f - tx ) + Ld.heights[j0][i0 + 1] * tx;
		const float b = Ld.heights[j0 + 1][i0] * ( 1.0f - tx ) + Ld.heights[j0 + 1][i0 + 1] * tx;
		*z = a * ( 1.0f - ty ) + b * ty;
		return true;
	};

	/* our coverage limit for a MIXED run (WW_CELL_FARLOD_OURS=x0,y0,x1,y1 cells): our files cover that
	 * rectangle only, the game's chunks the rest -- the boundary gate's arm */
	int lim[4] = { 0, 0, -1, -1 };
	{
		const QStringList p = qEnvironmentVariable( "WW_CELL_FARLOD_OURS" ).split( QLatin1Char( ',' ) );
		if ( p.size() == 4 )
			for ( int k = 0; k < 4; k++ )
				lim[k] = p[k].trimmed().toInt();
	}
	const bool haveLim = lim[2] >= lim[0] && lim[3] >= lim[1];

	/* ---- terrain rings, one call each: ring i's FOOTPRINT is what it was asked, what we built of it and the
	 * footprint inside; the next ring cuts the whole footprint, so ours and the game's chunks of one ring
	 * stand side by side and never under the next ring */
	std::vector<LodlFarRing> rings( defs.size() );
	std::vector<std::array<int, 4>> foot( defs.size() );
	std::vector<WwFarVanRing> van;
	/* FINALFIX: no shift. The cell scene is in WORLD units -- every welded shape carries Translation = origin
	 * (cellview.cpp, "Translation", origin) -- so far content placed at world - origin landed 67584 / 96256
	 * units off (over the block, the measured double ground and water lines added origin back and read 0). */
	const float shift[3] = { 0.0f, 0.0f, 0.0f };
	QString terr;
	bool terrOk = true;
	qint64 runVerts = 0;
	static const int kVanLevel[5] = { 4, 4, 8, 16, 32 };   // the game's authored level a ring's distance takes
	int fx0 = bx0, fy0 = by0, fx1 = bx1, fy1 = by1;
	for ( size_t i = 0; i < defs.size(); i++ ) {
		LodlFarRing & r = rings[i];
		r.lod = defs[i].lod;
		r.sheetDim = defs[i].sheetDim;
		r.x0 = cx - defs[i].reach;
		r.y0 = cy - defs[i].reach;
		r.x1 = cx + defs[i].reach;
		r.y1 = cy + defs[i].reach;
		r.cx0 = fx0;
		r.cy0 = fy0;
		r.cx1 = fx1;
		r.cy1 = fy1;
		r.cutGiven = true;
		r.nextLod = i + 1 < defs.size() ? defs[i + 1].lod : 0;
		if ( haveLim ) {
			r.lx0 = lim[0];
			r.ly0 = lim[1];
			r.lx1 = lim[2];
			r.ly1 = lim[3];
		}
		r.noCut = noCut && i == 0;
		r.exact = i == 0;	// FINALFIX: ring 0 at the LAND's rate only as far as its reach
		r.snapInner = !noSnap;
		if ( i == 0 )
			r.innerZ = innerZ;
		r.prefix = QStringLiteral( "FarLOD terrain r%1" ).arg( int( i ) );
		if ( ours && terrOk ) {
			std::vector<LodlFarRing> one( 1, r );
			const qint64 left = maxVerts > 0 ? std::max<qint64>( 1, maxVerts - runVerts ) : 0;
			if ( !nifAppendLodlFarRings( nif, iFar, lodlPath, shift, one, left, &terr ) ) {
				terrOk = false;
				say( QStringLiteral( "far lod: terrain REFUSED -- %1" ).arg( terr ) );
			} else {
				r = one[0];
				if ( r.built )
					runVerts += r.verts;
			}
		}
		int nx0 = std::min( r.x0, fx0 ), ny0 = std::min( r.y0, fy0 );
		int nx1 = std::max( r.x1, fx1 ), ny1 = std::max( r.y1, fy1 );
		if ( r.built ) {
			nx0 = std::min( nx0, r.ox0 );
			ny0 = std::min( ny0, r.oy0 );
			nx1 = std::max( nx1, r.ox1 );
			ny1 = std::max( ny1, r.oy1 );
		}
		WwFarVanRing v;
		v.level = kVanLevel[std::min( i, size_t( 4 ) )];
		/* FINALFIX: a ring we did not build is the game's -- whole chunks of its level, as the game draws
		 * them (ours snap to whole sheet tiles the same way). Measured: the Vanilla type's last ring stopped
		 * at cells -79..45 where ours reach -96..63, and the over view showed sky below the horizon there. */
		if ( !r.built ) {
			const int L = v.level;
			auto fl = [L]( int a ) { return ( a >= 0 ? a / L : -( ( -a + L - 1 ) / L ) ) * L; };
			nx0 = fl( nx0 );
			ny0 = fl( ny0 );
			nx1 = fl( nx1 ) + L - 1;
			ny1 = fl( ny1 ) + L - 1;
		}
		v.x0 = nx0;
		v.y0 = ny0;
		v.x1 = nx1;
		v.y1 = ny1;
		v.cx0 = fx0;
		v.cy0 = fy0;
		v.cx1 = fx1;
		v.cy1 = fy1;
		if ( r.built ) {
			v.ux0 = r.ox0;
			v.uy0 = r.oy0;
			v.ux1 = r.ox1;
			v.uy1 = r.oy1;
		}
		v.noOursCut = vanBoth;
		v.noCut = noCut && i == 0;
		v.tag = QStringLiteral( "r%1" ).arg( int( i ) );
		van.push_back( v );
		foot[i] = { nx0, ny0, nx1, ny1 };
		fx0 = nx0;
		fy0 = ny0;
		fx1 = nx1;
		fy1 = ny1;
	}

	// ---- the game's chunks where ours are not (all of them for the Vanilla type)
	{
		auto overBudget = [ws0, budgetMB]() {
			double w = 0, p = 0;
			memNow( w, p );
			return w - ws0 > budgetMB;
		};
		QString verr;
		if ( !wwFarVanAppend( nif, iFar, ws, shift, van, landZ, overBudget, &verr ) )
			say( QStringLiteral( "far lod: vanilla REFUSED -- %1" ).arg( verr ) );
		WwWaterRecord vdef;
		const bool haveVdef = world.defaultWaterType() != 0 && world.waterRecord( world.defaultWaterType(), vdef );
		qint64 sOurs = 0, sVan = 0, sNone = 0, sBoth = 0;
		for ( size_t i = 0; i < van.size(); i++ ) {
			const WwFarVanRing & v = van[i];
			int wReg = 0;
			if ( haveVdef )
				for ( int b : v.waterBlocks ) {
					wwCellWaterShape( nif, b, vdef );
					wReg++;
				}
			say( QStringLiteral( "far lod: source ring %1 level %2 type %3 cells %4: chunks ours %5, vanilla %6, none %7, both %8" )
				.arg( int( i ) ).arg( v.level ).arg( typeName )
				.arg( QStringLiteral( "%1,%2..%3,%4" ).arg( v.x0 ).arg( v.y0 ).arg( v.x1 ).arg( v.y1 ) )
				.arg( v.chunksOurs ).arg( v.chunksVanilla ).arg( v.chunksNone ).arg( v.chunksBoth )
				+ QStringLiteral( " -- vanilla %1 shapes, %2 terrain tris (%3 from clips), %4 object tris, %5 dropped, "
					"%6 seam vertices to the LAND (max step %7), %8 water shapes (%9 as the cell's water)" )
					.arg( v.shapes ).arg( v.terrainTris ).arg( v.clipTris ).arg( v.objectTris ).arg( v.droppedTris )
					.arg( v.snapped ).arg( v.snapMax, 0, 'f', 1 ).arg( int( v.waterBlocks.size() ) ).arg( wReg )
					+ QStringLiteral( ", %1 land shaders LOD landscape -> default, %2 skirt tris on the clip lines" ).arg( v.landRetyped ).arg( v.skirtTris )
				+ ( v.notes.isEmpty() ? QString() : QStringLiteral( " -- " ) + v.notes.join( QLatin1String( "; " ) ) ) );
			sOurs += v.chunksOurs;
			sVan += v.chunksVanilla;
			sNone += v.chunksNone;
			sBoth += v.chunksBoth;
		}
		say( QStringLiteral( "far lod: source total type %1: chunks ours %2, vanilla %3, none %4, both %5%6" )
			.arg( typeName ).arg( sOurs ).arg( sVan ).arg( sNone ).arg( sBoth )
			.arg( haveLim ? QStringLiteral( " (ours limited to %1,%2..%3,%4, WW_CELL_FARLOD_OURS)" )
				.arg( lim[0] ).arg( lim[1] ).arg( lim[2] ).arg( lim[3] ) : QString() )
			+ ( vanBoth ? QStringLiteral( " (RED vanboth: the game's drawn over ours)" ) : QString() ) );
	}

	qint64 tVerts = 0, tTris = 0, tCut = 0, tShapes = 0, tEst = 0;
	for ( size_t i = 0; ours && i < rings.size(); i++ ) {
		const LodlFarRing & r = rings[i];
		say( QStringLiteral( "far lod: ring %1 lod %2 (%3 a cell) %4 cells %5,%6..%7,%8: %9" )
			.arg( int( i ) ).arg( r.lod ).arg( r.n ).arg( r.built ? QStringLiteral( "built" ) : QStringLiteral( "NOT BUILT" ) )
			.arg( r.ox0 ).arg( r.oy0 ).arg( r.ox1 ).arg( r.oy1 )
			.arg( QStringLiteral( "%1 shapes, %2 verts (est %3), %4 tris, %5 cut tris, sheet %6 x%7 tiles%8" )
				.arg( r.shapes ).arg( r.verts ).arg( r.estVerts ).arg( r.tris ).arg( r.cutTris )
				.arg( r.sheetFile.isEmpty() ? QStringLiteral( "none" ) : r.sheetFile ).arg( r.sheetTiles )
				.arg( r.notes.isEmpty() ? QString() : QStringLiteral( " -- " ) + r.notes.join( QLatin1String( "; " ) ) ) ) );
		if ( r.built ) {
			tVerts += r.verts;
			tTris += r.tris;
			tCut += r.cutTris;
			tShapes += r.shapes;
			tEst += r.estVerts;
		}
	}
	if ( ours && !rings.empty() ) {
		const LodlFarRing & r0 = rings[0];
		say( QStringLiteral( "far lod: seam before max %1 mean %2, after max %3 mean %4 (%5 samples, snap %6)" )
			.arg( r0.seamBeforeMax, 0, 'f', 2 ).arg( r0.seamBeforeMean, 0, 'f', 2 )
			.arg( r0.seamAfterMax, 0, 'f', 2 ).arg( r0.seamAfterMean, 0, 'f', 2 )
			.arg( r0.seamSamples ).arg( noSnap ? QStringLiteral( "off (RED nosnap)" ) : QStringLiteral( "on" ) ) );
	}

	/* ---- the far WATER: the .lodl's water bodies over each built ring, the ring inside (the block for ring 0)
	 * left dry, each shape registered with its WATR record so the cell's water program draws it (lane WATER1's
	 * fo4_water.prog, the Cell lights row on) -- the game's LOD water, a plane the ground cuts. */
	qint64 wQuads = 0, wShapes = 0, wRegistered = 0;
	QSet<quint16> wBodies;
	if ( noWater ) {
		say( QStringLiteral( "far lod: water NONE (RED nowater)" ) );
	} else if ( ours && terrOk ) {
		static const int kWaterTexels[5] = { 8, 4, 2, 2, 1 };
		std::vector<LodlFarWater> wr;
		int hx0 = bx0, hy0 = by0, hx1 = bx1, hy1 = by1;
		for ( size_t i = 0; i < rings.size(); i++ ) {
			const LodlFarRing & r = rings[i];
			if ( !r.built ) {
				hx0 = foot[i][0];
				hy0 = foot[i][1];
				hx1 = foot[i][2];
				hy1 = foot[i][3];
				continue;
			}
			LodlFarWater w;
			w.x0 = r.ox0;
			w.y0 = r.oy0;
			w.x1 = r.ox1;
			w.y1 = r.oy1;
			w.cx0 = hx0;
			w.cy0 = hy0;
			w.cx1 = hx1;
			w.cy1 = hy1;
			w.noCut = waterCut && i == 0;
			w.texels = kWaterTexels[std::min( i, size_t( 4 ) )];
			wr.push_back( w );
			hx0 = foot[i][0];
			hy0 = foot[i][1];
			hx1 = foot[i][2];
			hy1 = foot[i][3];
		}
		QString werr;
		if ( !nifAppendLodlFarWater( nif, iFar, lodlPath, shift, wr, &werr ) ) {
			say( QStringLiteral( "far lod: water NONE -- %1" ).arg( werr ) );
		} else {
			WwWaterRecord def;
			const bool haveDef = world.defaultWaterType() != 0 && world.waterRecord( world.defaultWaterType(), def );
			QHash<quint32, int> recOk;   // 1 its own record, 0 the default's, -1 none
			QHash<quint32, WwWaterRecord> recs;
			for ( size_t i = 0; i < wr.size(); i++ ) {
				const LodlFarWater & w = wr[i];
				for ( const auto & [block, form] : w.blocks ) {
					if ( !recOk.contains( form ) ) {
						WwWaterRecord rec;
						if ( form != 0 && world.waterRecord( form, rec ) ) {
							recs.insert( form, rec );
							recOk.insert( form, 1 );
						} else {
							recOk.insert( form, haveDef ? 0 : -1 );
						}
					}
					const int how = recOk.value( form );
					if ( how < 0 )
						continue;
					wwCellWaterShape( nif, block, how > 0 ? recs.value( form ) : def );
					wRegistered++;
				}
				say( QStringLiteral( "far lod: water ring %1 (%2 a cell) cells %3,%4..%5,%6: %7 quads, %8 shapes, %9" )
					.arg( int( i ) ).arg( w.rate ).arg( w.x0 ).arg( w.y0 ).arg( w.x1 ).arg( w.y1 ).arg( w.quads ).arg( w.shapes )
					.arg( QStringLiteral( "%1 bodies, %2 wet texels, %3 grown onto the shore, cut %4,%5..%6,%7%8" )
						.arg( w.bodies.size() ).arg( w.wetTexels ).arg( w.grownTexels )
						.arg( w.cx0 ).arg( w.cy0 ).arg( w.cx1 ).arg( w.cy1 )
						.arg( w.noCut ? QStringLiteral( " (RED watercut: not cut)" ) : QString() ) ) );
				wQuads += w.quads;
				wShapes += w.shapes;
				wBodies.unite( w.bodies );
			}
			int own = 0, viaDef = 0, none = 0;
			for ( auto it = recOk.cbegin(); it != recOk.cend(); ++it )
				( it.value() > 0 ? own : it.value() == 0 ? viaDef : none )++;
			say( QStringLiteral( "far lod: water %1 quads, %2 shapes (%3 drawn as the cell's water), %4 bodies, "
				"%5 WATR forms: %6 their own record, %7 the worldspace default, %8 none" )
				.arg( wQuads ).arg( wShapes ).arg( wRegistered ).arg( wBodies.size() )
				.arg( recOk.size() ).arg( own ).arg( viaDef ).arg( none ) );
		}
	}

	/* THE DOUBLE GROUND, read back from the document: far terrain triangles whose centre lies over the loaded
	 * block. 0 is the cut working; the red (WW_CELL_FARLOD_RED=nocut) must count them. The far water the same
	 * way (red watercut). */
	qint64 doubleGround = 0, readTris = 0, waterOver = 0, waterTris = 0, vanOver = 0, vanTris = 0;
	QMap<QString, QString> vanWhere;   // the shapes a vanilla-over-ours count comes from, with their centres' box
	QHash<QString, std::array<float, 5>> vanBox;
	{
		// where OUR terrain draws, ring by ring: our built rect minus the footprint inside
		auto oursDraws = [&]( float x, float y ) {
			for ( size_t j = 0; j < rings.size(); j++ ) {
				const LodlFarRing & r = rings[j];
				if ( !r.built )
					continue;
				const bool inO = x > float( r.ox0 ) * kCell && x < float( r.ox1 + 1 ) * kCell
					&& y > float( r.oy0 ) * kCell && y < float( r.oy1 + 1 ) * kCell;
				const int p0 = j ? foot[j - 1][0] : bx0, q0 = j ? foot[j - 1][1] : by0;
				const int p1 = j ? foot[j - 1][2] : bx1, q1 = j ? foot[j - 1][3] : by1;
				const bool inP = x > float( p0 ) * kCell && x < float( p1 + 1 ) * kCell
					&& y > float( q0 ) * kCell && y < float( q1 + 1 ) * kCell;
				if ( inO && !inP )
					return true;
			}
			return false;
		};
		const float wx0 = float( bx0 ) * kCell, wy0 = float( by0 ) * kCell;
		const float wx1 = float( bx1 + 1 ) * kCell, wy1 = float( by1 + 1 ) * kCell;
		/* THE PLAN CRACKS: an 8-unit grid over the block grown by one cell; every far terrain triangle marks
		 * the grid points it covers seen from straight above, the block itself counts as covered. A point
		 * left bare is a crack the top view's sky census can only hit by chance (Vanilla top, 2 px). */
		const float gs = 8.0f, gx0 = wx0 - kCell, gy0 = wy0 - kCell;
		const int gw = int( ( wx1 - wx0 + 2.0f * kCell ) / gs ), gh = int( ( wy1 - wy0 + 2.0f * kCell ) / gs );
		std::vector<quint8> cover( size_t( gw ) * size_t( gh ), 0 );
		/* and finer along every cell line (chunk seams and clip lines lie on them): points 0.25, 1 and 3 units
		 * either side, every 2 units along */
		const float seamOff[6] = { -3.0f, -1.0f, -0.25f, 0.25f, 1.0f, 3.0f };
		const int nLx = int( ( wx1 - wx0 ) / kCell ) + 3, nLy = int( ( wy1 - wy0 ) / kCell ) + 3;
		const int alongX = int( ( wy1 - wy0 + 2.0f * kCell ) / 2.0f ), alongY = int( ( wx1 - wx0 + 2.0f * kCell ) / 2.0f );
		std::vector<quint8> seamX( size_t( nLx ) * 6 * size_t( alongX ), 0 ), seamY( size_t( nLy ) * 6 * size_t( alongY ), 0 );
		auto planMark = [&]( const Vector3 & a, const Vector3 & b2, const Vector3 & c2 ) {
			const float area = ( b2[0] - a[0] ) * ( c2[1] - a[1] ) - ( c2[0] - a[0] ) * ( b2[1] - a[1] );
			if ( std::fabs( area ) < 1e-3f )
				return;
			{
				const float sg0 = area > 0.0f ? 1.0f : -1.0f, tol0 = -1e-3f * std::fabs( area );
				auto inTri = [&]( float x, float y ) {
					const float e0 = sg0 * ( ( b2[0] - a[0] ) * ( y - a[1] ) - ( x - a[0] ) * ( b2[1] - a[1] ) );
					const float e1 = sg0 * ( ( c2[0] - b2[0] ) * ( y - b2[1] ) - ( x - b2[0] ) * ( c2[1] - b2[1] ) );
					const float e2 = sg0 * ( ( a[0] - c2[0] ) * ( y - c2[1] ) - ( x - c2[0] ) * ( a[1] - c2[1] ) );
					return e0 >= tol0 && e1 >= tol0 && e2 >= tol0;
				};
				const float mnx = std::min( { a[0], b2[0], c2[0] } ), mxx = std::max( { a[0], b2[0], c2[0] } );
				const float mny = std::min( { a[1], b2[1], c2[1] } ), mxy = std::max( { a[1], b2[1], c2[1] } );
				for ( int l = 0; l < nLx; l++ ) {
					const float lx = gx0 + float( l ) * kCell;
					if ( lx < mnx - 4.0f || lx > mxx + 4.0f )
						continue;
					const int k0 = std::max( 0, int( ( mny - gy0 ) / 2.0f ) - 1 ), k1 = std::min( alongX - 1, int( ( mxy - gy0 ) / 2.0f ) + 1 );
					for ( int o = 0; o < 6; o++ )
						for ( int k = k0; k <= k1; k++ )
							if ( inTri( lx + seamOff[o], gy0 + ( float( k ) + 0.5f ) * 2.0f ) )
								seamX[( size_t( l ) * 6 + size_t( o ) ) * size_t( alongX ) + size_t( k )] = 1;
				}
				for ( int l = 0; l < nLy; l++ ) {
					const float ly = gy0 + float( l ) * kCell;
					if ( ly < mny - 4.0f || ly > mxy + 4.0f )
						continue;
					const int k0 = std::max( 0, int( ( mnx - gx0 ) / 2.0f ) - 1 ), k1 = std::min( alongY - 1, int( ( mxx - gx0 ) / 2.0f ) + 1 );
					for ( int o = 0; o < 6; o++ )
						for ( int k = k0; k <= k1; k++ )
							if ( inTri( gx0 + ( float( k ) + 0.5f ) * 2.0f, ly + seamOff[o] ) )
								seamY[( size_t( l ) * 6 + size_t( o ) ) * size_t( alongY ) + size_t( k )] = 1;
				}
			}
			const int i0 = std::max( 0, int( std::floor( ( std::min( { a[0], b2[0], c2[0] } ) - gx0 ) / gs - 0.5f ) ) );
			const int i1 = std::min( gw - 1, int( std::ceil( ( std::max( { a[0], b2[0], c2[0] } ) - gx0 ) / gs - 0.5f ) ) );
			const int j0 = std::max( 0, int( std::floor( ( std::min( { a[1], b2[1], c2[1] } ) - gy0 ) / gs - 0.5f ) ) );
			const int j1 = std::min( gh - 1, int( std::ceil( ( std::max( { a[1], b2[1], c2[1] } ) - gy0 ) / gs - 0.5f ) ) );
			const float sg = area > 0.0f ? 1.0f : -1.0f, tol = -1e-3f * std::fabs( area );
			for ( int j = j0; j <= j1; j++ )
				for ( int i = i0; i <= i1; i++ ) {
					const float x = gx0 + ( float( i ) + 0.5f ) * gs, y = gy0 + ( float( j ) + 0.5f ) * gs;
					const float e0 = sg * ( ( b2[0] - a[0] ) * ( y - a[1] ) - ( x - a[0] ) * ( b2[1] - a[1] ) );
					const float e1 = sg * ( ( c2[0] - b2[0] ) * ( y - b2[1] ) - ( x - b2[0] ) * ( c2[1] - b2[1] ) );
					const float e2 = sg * ( ( a[0] - c2[0] ) * ( y - c2[1] ) - ( x - c2[0] ) * ( a[1] - c2[1] ) );
					if ( e0 >= tol && e1 >= tol && e2 >= tol )
						cover[size_t( j ) * size_t( gw ) + size_t( i )] = 1;
				}
		};
		for ( int b = firstFarBlock + 1; b < nif->getBlockCount(); b++ ) {
			const QModelIndex iB = nif->getBlockIndex( b );
			if ( !nif->blockInherits( iB, "BSTriShape" ) )
				continue;
			const QString bn = nif->get<QString>( iB, "Name" );
			const bool isWater = bn.startsWith( QLatin1String( "FarLOD water" ) );
			if ( nif->get<quint32>( iB, "Flags" ) & 1u )
				continue;   // hidden (a game chunk's shape dropped whole)
			// the game's chunks keep their own vertex layout (half precision is possible)
			const bool fullPrec = ( ( nif->get<BSVertexDesc>( iB, "Vertex Desc" ).Value() >> 44 ) & VF_FULLPREC ) != 0;
			if ( !isWater && !bn.startsWith( QLatin1String( "FarLOD terrain" ) ) )
				continue;
			const Transform xf( nif, iB );   // FarLOD's own transform is the identity
			const bool isVan = bn.startsWith( QLatin1String( "FarLOD terrain v " ) );
			const QModelIndex iVD = nif->getIndex( iB, "Vertex Data" );
			const QModelIndex iTri = nif->getIndex( iB, "Triangles" );
			if ( !iVD.isValid() || !iTri.isValid() )
				continue;
			const QVector<Triangle> tris = nif->getArray<Triangle>( iTri );
			const int nv = nif->rowCount( iVD );
			std::vector<Vector3> p( size_t( std::max( nv, 0 ) ) );
			for ( int v = 0; v < nv; v++ )
				p[size_t( v )] = fullPrec ? nif->get<Vector3>( nif->index( v, 0, iVD ), "Vertex" )
					: Vector3( nif->get<HalfVector3>( nif->index( v, 0, iVD ), "Vertex" ) );
			for ( const Triangle & tr : tris ) {
				if ( tr.v1() >= nv || tr.v2() >= nv || tr.v3() >= nv )
					continue;
				( isWater ? waterTris : readTris )++;
				const Vector3 c = xf * ( ( p[tr.v1()] + p[tr.v2()] + p[tr.v3()] ) / 3.0f );   // world units (no shift)
				if ( !isWater )
					planMark( xf * p[tr.v1()], xf * p[tr.v2()], xf * p[tr.v3()] );
				if ( c[0] > wx0 && c[0] < wx1 && c[1] > wy0 && c[1] < wy1 )
					( isWater ? waterOver : doubleGround )++;
				if ( isVan ) {
					vanTris++;
					if ( oursDraws( c[0], c[1] ) ) {
						vanOver++;
						if ( vanWhere.contains( bn ) || vanWhere.size() < 6 ) {
							std::array<float, 5> & e = vanBox[bn];
							if ( e[4] == 0.0f )
								e = { c[0], c[1], c[0], c[1], 0.0f };
							e[0] = std::min( e[0], c[0] );
							e[1] = std::min( e[1], c[1] );
							e[2] = std::max( e[2], c[0] );
							e[3] = std::max( e[3], c[1] );
							e[4] += 1.0f;
							vanWhere.insert( bn, QStringLiteral( "%1 x%2 centres in cells %3,%4..%5,%6" ).arg( bn ).arg( double( e[4] ), 0, 'f', 0 )
								.arg( double( e[0] / kCell ), 0, 'f', 3 ).arg( double( e[1] / kCell ), 0, 'f', 3 )
								.arg( double( e[2] / kCell ), 0, 'f', 3 ).arg( double( e[3] / kCell ), 0, 'f', 3 ) );
						}
					}
				}
			}
		}
		qint64 bare = 0, pts = 0;
		QStringList bareAt;
		for ( int j = 0; j < gh; j++ )
			for ( int i = 0; i < gw; i++ ) {
				const float x = gx0 + ( float( i ) + 0.5f ) * gs, y = gy0 + ( float( j ) + 0.5f ) * gs;
				if ( x > wx0 && x < wx1 && y > wy0 && y < wy1 )
					continue;   // the loaded block draws its own LAND
				pts++;
				if ( cover[size_t( j ) * size_t( gw ) + size_t( i )] )
					continue;
				bare++;
				if ( bareAt.size() < 8 )
					bareAt << QStringLiteral( "(%1,%2)" ).arg( double( x / kCell ), 0, 'f', 4 ).arg( double( y / kCell ), 0, 'f', 4 );
			}
		auto inBlock = [&]( float x, float y ) { return x > wx0 && x < wx1 && y > wy0 && y < wy1; };
		auto bareSeam = [&]( float x, float y ) {
			bare++;
			if ( bareAt.size() < 8 )
				bareAt << QStringLiteral( "(%1,%2)" ).arg( double( x / kCell ), 0, 'f', 5 ).arg( double( y / kCell ), 0, 'f', 5 );
		};
		for ( int l = 0; l < nLx; l++ )
			for ( int o = 0; o < 6; o++ )
				for ( int k = 0; k < alongX; k++ ) {
					const float x = gx0 + float( l ) * kCell + seamOff[o], y = gy0 + ( float( k ) + 0.5f ) * 2.0f;
					if ( inBlock( x, y ) || x < gx0 || x > gx0 + float( gw ) * gs )
						continue;
					pts++;
					if ( !seamX[( size_t( l ) * 6 + size_t( o ) ) * size_t( alongX ) + size_t( k )] )
						bareSeam( x, y );
				}
		for ( int l = 0; l < nLy; l++ )
			for ( int o = 0; o < 6; o++ )
				for ( int k = 0; k < alongY; k++ ) {
					const float x = gx0 + ( float( k ) + 0.5f ) * 2.0f, y = gy0 + float( l ) * kCell + seamOff[o];
					if ( inBlock( x, y ) || y < gy0 || y > gy0 + float( gh ) * gs )
						continue;
					pts++;
					if ( !seamY[( size_t( l ) * 6 + size_t( o ) ) * size_t( alongY ) + size_t( k )] )
						bareSeam( x, y );
				}
		say( QStringLiteral( "far lod: plan cracks %1 of %2 grid points within a cell of the block (8 units, and 0.25-3 units either side of each cell line; far terrain seen from above)%3" )
			.arg( bare ).arg( pts ).arg( bareAt.isEmpty() ? QString() : QStringLiteral( "; at cells %1" ).arg( bareAt.join( QLatin1Char( ' ' ) ) ) ) );
	}
	say( QStringLiteral( "far lod: double ground %1 of %2 far terrain triangles over the loaded block %3,%4..%5,%6%7" )
		.arg( doubleGround ).arg( readTris ).arg( bx0 ).arg( by0 ).arg( bx1 ).arg( by1 )
		.arg( noCut ? QStringLiteral( " (RED nocut)" ) : QString() ) );
	say( QStringLiteral( "far lod: water over block %1 of %2 far water triangles%3" ).arg( waterOver ).arg( waterTris )
		.arg( waterCut ? QStringLiteral( " (RED watercut)" ) : QString() ) );
	say( QStringLiteral( "far lod: vanilla over ours %1 of %2 vanilla terrain triangles%3%4" ).arg( vanOver ).arg( vanTris )
		.arg( vanBoth ? QStringLiteral( " (RED vanboth)" ) : QString() )
		.arg( vanWhere.isEmpty() ? QString() : QStringLiteral( "; from %1" ).arg( QStringList( vanWhere.values() ).join( QStringLiteral( ", " ) ) ) ) );

	double ws1 = 0, pk1 = 0;
	memNow( ws1, pk1 );
	say( QStringLiteral( "far lod: terrain %1 shapes, %2 verts, %3 tris, %4 cut tris; memory est %5 MB, "
		"working set %6 -> %7 MB" ).arg( tShapes ).arg( tVerts ).arg( tTris ).arg( tCut )
		.arg( double( tEst ) * kBytesPerVert / ( 1024.0 * 1024.0 ), 0, 'f', 0 )
		.arg( ws0, 0, 'f', 0 ).arg( ws1, 0, 'f', 0 ) );

	// ---- the tree cards: the chunk manifests (dim 8) around the block, out to the tree distance
	const QFileInfo li( lodlPath );
	const float ctrX = origin[0], ctrY = origin[1];
	QSet<quint32> cardRefs;
	int cards = 0;
	if ( ours ) {
		const int R = cellsTo( treeDist > 0.0f ? treeDist : dMax ) + 1;
		const int dim = 8;
		QStringList chunks;
		for ( int y = floorDiv( cy - R, dim ) * dim; y <= cy + R; y += dim )
			for ( int x = floorDiv( cx - R, dim ) * dim; x <= cx + R; x += dim ) {
				const QString base = li.absoluteDir().filePath(
					QStringLiteral( "%1.%2.%3.%4.BTO" ).arg( ws ).arg( dim ).arg( x ).arg( y ) );
				if ( QFileInfo::exists( base + QStringLiteral( ".manifest.txt" ) ) )
					chunks << base;
			}
		QStringList cn;
		cards = ImpostorChunk::armCellFar( nif, chunks, shift, ctrX, ctrY, treeDist, bx0, by0, bx1, by1,
			&cardRefs, &cn );
		for ( const QString & c : cn )
			say( c.startsWith( QLatin1String( "far lod:" ) ) ? c : QStringLiteral( "far lod: cards: " ) + c );
	}

	// ---- objects, ring by ring: the authored slot of each, the ring inside cut out
	const QString lodi = li.absoluteDir().filePath( li.completeBaseName() + QStringLiteral( ".lodi" ) );
	qint64 oPlaced = 0, oTris = 0, oSway = 0, oDropped = 0, oRefs = 0;
	// lane FARLOD2: per ring, for the census against the game's object LOD in the same region
	std::vector<qint64> ringTris( rings.size(), 0 ), ringPlaced( rings.size(), 0 );
	std::vector<QString> ringState( rings.size(), QStringLiteral( "not reached" ) );
	std::vector<std::array<int, 8>> ringRegion( rings.size(), std::array<int, 8>{ { 0, 0, -1, -1, 0, 0, -1, -1 } } );
	{	// every ring's rectangle and hole, whether or not its objects get built (the hole chain of the loop below)
		int gx0 = bx0, gy0 = by0, gx1 = bx1, gy1 = by1;
		for ( size_t i = 0; i < rings.size(); i++ ) {
			if ( rings[i].built )
				ringRegion[i] = { { rings[i].ox0, rings[i].oy0, rings[i].ox1, rings[i].oy1, gx0, gy0, gx1, gy1 } };
			gx0 = foot[i][0];
			gy0 = foot[i][1];
			gx1 = foot[i][2];
			gy1 = foot[i][3];
		}
	}
	if ( !ours ) {
		say( QStringLiteral( "far lod: objects ours off (type %1): the game's .bto only" ).arg( typeName ) );
	} else if ( !QFileInfo( lodi ).isFile() ) {
		say( QStringLiteral( "far lod: objects NONE -- no %1" ).arg( QDir::toNativeSeparators( lodi ) ) );
	} else {
		int hx0 = bx0, hy0 = by0, hx1 = bx1, hy1 = by1;
		for ( size_t i = 0; i < rings.size(); i++ ) {
			const LodlFarRing & r = rings[i];
			if ( !r.built ) {
				hx0 = foot[i][0];
				hy0 = foot[i][1];
				hx1 = foot[i][2];
				hy1 = foot[i][3];
				continue;
			}
			double wsR = 0, pkR = 0;
			memNow( wsR, pkR );
			if ( wsR - ws0 > budgetMB ) {
				say( QStringLiteral( "far lod: objects ring %1 NOT BUILT -- working set +%2 MB is past the far "
					"field's budget of %3 MB" ).arg( int( i ) ).arg( wsR - ws0, 0, 'f', 0 ).arg( budgetMB, 0, 'f', 0 ) );
				ringState[i] = QStringLiteral( "NOT BUILT (budget)" );
				break;
			}
			LodiSceneSpec os;
			os.x0 = r.ox0;
			os.y0 = r.oy0;
			os.x1 = r.ox1;
			os.y1 = r.oy1;
			os.haveRegion = true;
			os.level = 0;
			os.valid = true;
			os.haveHole = true;
			os.hx0 = hx0;
			os.hy0 = hy0;
			os.hx1 = hx1;
			os.hy1 = hy1;
			os.slot = defs[i].slot;
			os.namePrefix = QStringLiteral( "FarLOD r%1 " ).arg( int( i ) );
			os.shift[0] = shift[0];
			os.shift[1] = shift[1];
			os.shift[2] = shift[2];
			os.dropTreesBeyond = treeDist;   // 0 = no cut (lane FARLOD2)
			os.sideMesh = true;              // lane FARLOD2: arrays beside the document, not rows
			os.dropX = ctrX;
			os.dropY = ctrY;
			os.sway = swayAmp() > 0.0f;
			os.skipRefs = &cardRefs;
			/* FINALFIX: the ring's triangles are counted before the document grows; a ring that would take the
			 * far field past its budget is not written. Measured on the merged build (g3, Commonwealth -17,23):
			 * ring 0 636345 tris +1514 MB, ring 1 1282884 tris +3131 MB -- 2.4 KB a triangle, rounded up. */
			const bool sideMesh = cellMeshOn();
			const double kBytesPerTri = sideMesh ? kSideBytesPerTri : 2560.0;
			const double leftMB = budgetMB - ( wsR - ws0 );
			os.maxTris = std::max<qint64>( 1, qint64( leftMB * 1024.0 * 1024.0 / kBytesPerTri ) );
			LodiAppendCounts oc;
			QString oerr, onotes;
			if ( !nifAppendLodiObjects( nif, iFar, lodi, os, &oerr, &onotes, &oc ) ) {
				say( QStringLiteral( "far lod: objects ring %1 REFUSED -- %2" ).arg( int( i ) ).arg( oerr ) );
				ringState[i] = QStringLiteral( "REFUSED" );
				break;
			}
			if ( oc.overBudget ) {
				say( QStringLiteral( "far lod: objects ring %1 NOT BUILT -- %2 tris (about %3 MB) would take the far "
					"field past its budget of %4 MB (%5 MB left)" ).arg( int( i ) ).arg( oc.tris )
					.arg( double( oc.tris ) * kBytesPerTri / ( 1024.0 * 1024.0 ), 0, 'f', 0 )
					.arg( budgetMB, 0, 'f', 0 ).arg( leftMB, 0, 'f', 0 ) );
				ringState[i] = QStringLiteral( "NOT BUILT (budget)" );
				break;
			}
			double wsA = 0, pkA = 0;
			memNow( wsA, pkA );
			say( QStringLiteral( "far lod: objects ring %1 slot %2: %3 placed, %4 in the hole, %5 trees %6, "
				"%7 drawn as cards, %8 tris, %9" ).arg( int( i ) ).arg( os.slot ).arg( oc.placed ).arg( oc.holeSkipped )
				.arg( oc.treesDropped ).arg( treeCut ).arg( oc.refsSkipped ).arg( oc.tris )
				.arg( QStringLiteral( "%1 sway verts, working set %2 -> %3 MB (%4 B a triangle measured, %5 %6)" )
					.arg( oc.swayVerts ).arg( wsR, 0, 'f', 0 ).arg( wsA, 0, 'f', 0 )
					.arg( oc.tris > 0 ? ( wsA - wsR ) * 1024.0 * 1024.0 / double( oc.tris ) : 0.0, 0, 'f', 0 )
					.arg( sideMesh ? QStringLiteral( "arrays beside the document" ) : QStringLiteral( "document rows" ) )
					.arg( QStringLiteral( "estimate %1" ).arg( kBytesPerTri, 0, 'f', 0 ) ) ) );
			ringTris[i] = oc.tris;
			ringPlaced[i] = oc.placed;
			ringState[i] = QStringLiteral( "built" );
			oPlaced += oc.placed;
			oTris += oc.tris;
			oSway += oc.swayVerts;
			oDropped += oc.treesDropped;
			oRefs += oc.refsSkipped;
			hx0 = foot[i][0];
			hy0 = foot[i][1];
			hx1 = foot[i][2];
			hy1 = foot[i][3];
		}
	}

	/* lane FARLOD2: THE CENSUS. Every object ring against the game's own object LOD (.bto, the game's level for
	 * that ring) over the same rectangle minus the same hole. A ring the game fills and we leave empty is the
	 * defect this lane exists to end; the line says which by name. */
	if ( ours && QFileInfo( lodi ).isFile() ) {
		static const int kGameLevel[5] = { 4, 4, 8, 16, 32 };
		for ( size_t i = 0; i < rings.size(); i++ ) {
			const std::array<int, 8> & g = ringRegion[i];
			int chunksRead = 0;
			const qint64 game = g[2] < g[0] ? 0 : wwFarVanObjectCensus( ws, kGameLevel[std::min( i, size_t( 4 ) )],
				g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7], &chunksRead );
			say( QStringLiteral( "far lod: census ring %1: ours %2 tris %3 placed (%4) | the game's .bto %5 tris from %6 chunks "
				"at level %7, same region%8" ).arg( int( i ) ).arg( ringTris[i] ).arg( ringPlaced[i] ).arg( ringState[i] )
				.arg( game ).arg( chunksRead ).arg( kGameLevel[std::min( i, size_t( 4 ) )] )
				.arg( game > 0 && ringTris[i] == 0 ? QStringLiteral( " -- MISSING: the game draws objects here, we draw none" )
					: QString() ) );
		}
	}

	// ---- the draw's view of it
	FarDoc fd;
	fd.origin[0] = origin[0];
	fd.origin[1] = origin[1];
	fd.origin[2] = origin[2];
	fd.bx0 = bx0;
	fd.by0 = by0;
	fd.bx1 = bx1;
	fd.by1 = by1;
	fd.cards = cards;
	int outer = -1;
	for ( size_t i = 0; i < rings.size(); i++ )
		if ( rings[i].built || van[i].chunksVanilla + van[i].chunksBoth > 0 )
			outer = int( i );
	if ( outer >= 0 ) {
		const std::array<int, 4> & r = foot[size_t( outer )];
		float m = 0.0f;
		for ( int c = 0; c < 4; c++ ) {
			const float x = float( ( c & 1 ) ? r[2] + 1 : r[0] ) * kCell - origin[0];
			const float y = float( ( c & 2 ) ? r[3] + 1 : r[1] ) * kCell - origin[1];
			m = std::max( m, std::sqrt( x * x + y * y ) );
		}
		fd.reach = m;
		fd.fx0 = r[0];
		fd.fy0 = r[1];
		fd.fx1 = r[2];
		fd.fy1 = r[3];
	}
	say( farlod1CullRegister( nif, firstFarBlock ) );   // lane SUNCELL1's cull table, when merged
	docs().insert( nif, fd );
	if ( !hooked().contains( nif ) ) {
		hooked().insert( nif );
		const void * key = nif;
		// another file opened in this window empties the document: its far field goes with it
		QObject::connect( nif, &QAbstractItemModel::modelReset, nif, [nif, key]() {
			if ( nif->getBlockCount() == 0 && docs().remove( key ) )
				ImpostorChunk::forgetCellFar();
		} );
		QObject::connect( nif, &QObject::destroyed, [key]() {
			docs().remove( key );
			hooked().remove( key );
		} );
	}

	double ws2 = 0, pk2 = 0;
	memNow( ws2, pk2 );
	say( QStringLiteral( "far lod: total %1 rings, reach %2 units, terrain %3 verts, objects %4 placed %5 tris "
		"(%6 sway verts, %7 trees past the tree distance, %8 as cards), cards %9" )
		.arg( outer + 1 ).arg( double( fd.reach ), 0, 'f', 0 ).arg( tVerts ).arg( oPlaced ).arg( oTris )
		.arg( oSway ).arg( oDropped ).arg( oRefs ).arg( cards ) );
	say( QStringLiteral( "far lod: memory budget %1 MB, working set %2 -> %3 MB (+%4), peak %5 MB, %6 ms" )
		.arg( budgetMB, 0, 'f', 0 ).arg( ws0, 0, 'f', 0 ).arg( ws2, 0, 'f', 0 ).arg( ws2 - ws0, 0, 'f', 0 )
		.arg( pk2, 0, 'f', 0 ).arg( clock.elapsed() ) );

	QString out;
	for ( const QString & l : std::as_const( L ) )
		out += QStringLiteral( "  " ) + l + QLatin1Char( '\n' );
	return out;
}

CellFarStack cellFarLodStackFor( const QString & ws, const QString & lodlPath, const QString & nearPlugins )
{
	CellFarStack r;
	QString why;
	const int type = farlod1TypeFor( !lodlPath.isEmpty(), &why );
	const QStringList nearList = nearPlugins.split( QLatin1Char( ',' ), Qt::SkipEmptyParts );
	if ( type != WwFarLodFo4cs || lodlPath.isEmpty() ) {
		r.line = QStringLiteral( "far lod: stack not checked -- type %1 (%2): the near cells keep their load order (%3 plugins)" )
			.arg( type == WwFarLodVanilla ? QStringLiteral( "vanilla" ) : QStringLiteral( "fo4cs, no %1.lodl" ).arg( ws ) )
			.arg( why ).arg( nearList.size() );
		return r;
	}
	const QFileInfo li( lodlPath );
	const QString lodb = li.absoluteDir().filePath( li.completeBaseName() + QStringLiteral( ".lodb" ) );
	LodgenLedger led;
	QString lerr;
	if ( !QFileInfo( lodb ).isFile() || !lodgenReadLedger( lodb, &led, &lerr ) || led.plugins.isEmpty() ) {
		r.line = QStringLiteral( "far lod: stack MISMATCH UNKNOWN -- NOT adopted: the bake records no load order (%1%2); "
			"the near cells keep theirs (%3 plugins)" ).arg( QDir::toNativeSeparators( lodb ) )
			.arg( lerr.isEmpty() ? QString() : QStringLiteral( ": " ) + lerr ).arg( nearList.size() );
		return r;
	}
	// what moved: names in order (cheap -- the byte hashes are the bake record's own business)
	QStringList nearNames, bakeNames;
	for ( const QString & p : nearList )
		nearNames << QFileInfo( p.trimmed() ).fileName().toLower();
	for ( const LodbPlugin & p : std::as_const( led.plugins ) )
		bakeNames << p.name.toLower();
	QStringList moved;
	for ( const QString & n : bakeNames )
		if ( !nearNames.contains( n ) )
			moved << QStringLiteral( "%1 missing from the near cells" ).arg( n );
	for ( const QString & n : nearNames )
		if ( !bakeNames.contains( n ) )
			moved << QStringLiteral( "%1 not in the bake" ).arg( n );
	if ( moved.isEmpty() && nearNames != bakeNames )
		moved << QStringLiteral( "the same plugins in another order" );
	QStringList bakeRes;
	for ( const LodbResource & res : std::as_const( led.resources ) )
		bakeRes << res.path;
	const QStringList nowRes = lodgenResources();
	const bool resSame = bakeRes.isEmpty() || nowRes == bakeRes;
	if ( moved.isEmpty() && resSame ) {
		r.line = QStringLiteral( "far lod: stack match -- near and far from one load order: %1 plugins, %2 resource roots "
			"(the bake's record %3)" ).arg( bakeNames.size() ).arg( nowRes.size() ).arg( QDir::toNativeSeparators( lodb ) );
		return r;
	}
	const QString what = QStringLiteral( "the near cells load %1 plugins, the far LOD was baked from %2 (%3%4)%5" )
		.arg( nearNames.size() ).arg( bakeNames.size() ).arg( moved.size() ).arg( moved.isEmpty() ? QString()
			: QStringLiteral( " differences: " ) + QStringList( moved.mid( 0, 4 ) ).join( QStringLiteral( "; " ) )
				+ ( moved.size() > 4 ? QStringLiteral( "; ..." ) : QString() ) )
		.arg( resSame ? QString() : QStringLiteral( "; resource stack %1 roots now, %2 in the bake" ).arg( nowRes.size() ).arg( bakeRes.size() ) );
	if ( qgetenv( "WW_CELL_FARLOD_RED" ) == "ownstack" ) {
		r.line = QStringLiteral( "far lod: stack MISMATCH -- %1; NOT adopted (RED ownstack)" ).arg( what );
		return r;
	}
	// refuse rather than half-adopt: every recorded plugin and every recorded folder must still be on disk
	QStringList paths, missing;
	for ( const LodbPlugin & p : std::as_const( led.plugins ) ) {
		if ( !QFileInfo( p.path ).isFile() )
			missing << QDir::toNativeSeparators( p.path );
		paths << p.path;
	}
	for ( const LodbResource & res : std::as_const( led.resources ) )
		if ( !QFileInfo::exists( res.path ) )
			missing << QDir::toNativeSeparators( res.path );
	if ( !missing.isEmpty() ) {
		r.line = QStringLiteral( "far lod: stack MISMATCH -- %1; NOT adopted: %2 of the bake's files are gone (%3)" )
			.arg( what ).arg( missing.size() ).arg( QStringList( missing.mid( 0, 3 ) ).join( QStringLiteral( "; " ) ) );
		return r;
	}
	r.adopt = true;
	r.plugins = paths.join( QLatin1Char( ',' ) );
	r.resources = bakeRes;
	r.line = QStringLiteral( "far lod: stack ADOPTED -- %1; the near cells now load the bake's %2 plugins and %3 resource "
		"roots (%4)" ).arg( what ).arg( paths.size() ).arg( bakeRes.size() ).arg( QDir::toNativeSeparators( lodb ) );
	return r;
}

void cellFarLodAdoptResources( const QStringList & resources )
{
	if ( resources.isEmpty() || lodgenResources() == resources )
		return;
	lodgenSetResources( resources );
	QStringList view = lodgenResourceSearchPaths();
	for ( const QString & f : Game::GameManager::folders( Game::FALLOUT_4 ) )
		if ( !view.contains( f, Qt::CaseInsensitive ) )
			view.append( f );
	Game::GameManager::update_folders( Game::FALLOUT_4, view );
	Game::GameManager::close_resources();
}

float wwCellFarLodReach( const Scene * scene )
{
	const FarDoc * f = farFor( scene );
	return f ? f->reach : 0.0f;
}

Vector3 wwCellFarLodOrigin( const Scene * scene )
{
	const FarDoc * f = farFor( scene );
	return f ? Vector3( f->origin[0], f->origin[1], f->origin[2] ) : Vector3();
}

void wwCellFarLodFrame( Scene * scene )
{
	static bool wasSet = false;
	if ( !scene || !scene->renderer )
		return;
	const FarDoc * f = farFor( scene );
	const float amp = ( f && f->reach > 0.0f ) ? swayAmp() : 0.0f;
	if ( amp <= 0.0f && !wasSet )
		return;   // nothing ever set: not one GL call (the off path)
	Renderer * r = scene->renderer;
	const float t = swayTime();
	/* every program on fo4_default.vert a far shape can draw with: the legacy path, the PBR path, and SUNCELL1's
	 * cascade variants (absent before that merge: useProgram returns null and the name is skipped) */
	/* the loaded block, in view space: the sway is the FAR field's, so the cell's own tree-animation shapes inside
	 * the block stand as they did with the row off (g10 measured them swaying: 68,366 sky-band px on the eye view) */
	float blk[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ( f ) {
		const float kC = 4096.0f;
		const Vector3 c = scene->view * Vector3( float( f->bx0 + f->bx1 + 1 ) * 0.5f * kC, float( f->by0 + f->by1 + 1 ) * 0.5f * kC, 0.0f );
		blk[0] = c[0]; blk[1] = c[1]; blk[2] = c[2];
		blk[3] = float( std::max( f->bx1 - f->bx0, f->by1 - f->by0 ) + 1 ) * 0.5f * kC * scene->view.scale;
	}
	QStringList set;
	for ( const char * name : { "fo4_cell.prog", "fo4_default.prog", "fo4_fog.prog", "pbrm_cell.prog", "pbrm_default.prog",
			"pbrm_csm.prog", "fo4_cellcsm.prog", "fo4_csm.prog", "fo4_fogcsm.prog", "pbrm_cellcsm.prog" } ) {
		if ( auto prog = r->useProgram( name ) ) {
			prog->uni1f( "farSwayAmp", amp );
			prog->uni1f( "farSwayTime", t );
			prog->uni4f( "farSwayBlock", FloatVector4( blk[0], blk[1], blk[2], blk[3] ) );
			if ( prog->uniLocation( "farSwayAmp" ) >= 0 )
				set << QString::fromLatin1( name );
		}
	}
	r->stopProgram();
	wasSet = amp > 0.0f;
	static QString said;
	const QString line = QStringLiteral( "far lod: sway amp %1 on %2 programs: %3" ).arg( double( amp ), 0, 'f', 1 )
		.arg( set.size() ).arg( set.join( QLatin1Char( ' ' ) ) );
	if ( line != said ) {
		said = line;
		qInfo().noquote() << line;
	}
}

int wwCellFarLodCards( Scene * scene, bool insideHdr, bool hdrFrame )
{
	const FarDoc * f = farFor( scene );
	if ( !f || f->cards <= 0 )
		return 0;
	// the red control: the Phase 1 path, after the resolve and lit by the viewer alone
	const bool redAfter = qgetenv( "WW_CELL_FARLOD_RED" ) == "cardsafter";
	if ( redAfter ? insideHdr : ( insideHdr != hdrFrame ) )
		return 0;
	ImpostorDraw::Options o;
	o.cellLit = !redAfter;
	/* WW_CELL_FARLOD_CARDSWAY=0 (harness only): the cards stand still, so the PBR sway gate (g9) sees the
	 * mesh trees alone -- the cards sway on their own path and would move both its arms. */
	const float amp = qgetenv( "WW_CELL_FARLOD_CARDSWAY" ) == "0" ? 0.0f : swayAmp();
	if ( amp > 0.0f ) {
		// the card's own sway is a UV shear (impostordraw.h), a fraction, not units
		o.swayAmplitude = std::min( 0.05f, amp * 0.002f );
		o.swayPhase = swayTime() * 1.3f;
	}
	if ( insideHdr && wwCellHdrActive() && scene->renderer ) {
		/* the HDR frame sorts its pixels by the stencil (gl/cellhdr.h): 1 = linear light from a cell program, toned
		 * once at the resolve. The cards write linear light (cellIsLinear), so they mark 1, as the far bulbs do. */
		auto fn = scene->renderer->fn;
		fn->glEnable( GL_STENCIL_TEST );
		fn->glStencilMask( 0x03 );
		fn->glStencilFunc( GL_ALWAYS, 1, 0xFF );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	}
	const int drawn = ImpostorChunk::drawCellFar( scene, o );
	static QString said;
	const QString line = QStringLiteral( "far lod: cards pass %1 -- %2 drawn, %3, %4" )
		.arg( redAfter ? QStringLiteral( "AFTER the frame (RED cardsafter)" ) : QStringLiteral( "lit" ) )
		.arg( drawn )
		.arg( o.cellLit ? QStringLiteral( "impostor_cell (cell lights, fog, imagespace)" ) : QStringLiteral( "impostor_oct (viewer light)" ) )
		.arg( insideHdr ? QStringLiteral( "inside the HDR frame" ) : ( hdrFrame ? QStringLiteral( "after the HDR resolve" )
			: QStringLiteral( "no HDR frame" ) ) );
	if ( line != said ) {
		said = line;
		qInfo().noquote() << line;
	}
	return drawn;
}

double wwCellFarLodNear( double nr )
{
	const float forced = envFloat( "WW_CELL_FARLOD_NEAR", 0.0f );
	return forced > 0.0f ? double( forced ) : std::max( nr, 16.0 );
}

void wwCellFarLodPlanes( double nearPlane, double farPlane )
{
	static QString said;
	/* The depth step of a 24-bit buffer at distance d under this perspective: d^2 (f - n) / (n f 2^24).
	 * The near gate holds it under 1 unit at 1000 (z-fight free where the cell's decals and contacts are);
	 * the red near (WW_CELL_FARLOD_NEAR=0.01) puts it at 6. */
	auto step = [nearPlane, farPlane]( double d ) {
		return d * d * ( farPlane - nearPlane ) / ( nearPlane * farPlane * 16777216.0 );
	};
	const QString line = QStringLiteral( "far lod: planes near %1 far %2, depth step %3 at 1000, %4 at 10000, %5 at 100000" )
		.arg( nearPlane, 0, 'f', 2 ).arg( farPlane, 0, 'f', 0 )
		.arg( step( 1000.0 ), 0, 'f', 4 ).arg( step( 10000.0 ), 0, 'f', 3 ).arg( step( 100000.0 ), 0, 'f', 1 );
	if ( line != said ) {
		said = line;
		qInfo().noquote() << line;
	}
}

void wwCellFarLodSkyCensus( Scene * scene )
{
	if ( !qEnvironmentVariableIsSet( "WW_CELL_FARLOD_SKYCENSUS" ) || !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	static QString said;
	auto say = []( const QString & line ) {
		if ( line != said ) {
			said = line;
			qInfo().noquote() << line;
		}
	};
	const float P00 = r->globalUniforms->projectionMatrix[0][0];
	const float P11 = r->globalUniforms->projectionMatrix[1][1];
	if ( r->globalUniforms->projectionMatrix[3][3] == 1.0f || P00 == 0.0f || P11 == 0.0f ) {
		say( QStringLiteral( "far lod: sky census UNREADABLE (not a perspective view)" ) );
		return;
	}
	while ( fn->glGetError() != GL_NO_ERROR ) {}

	GLint vp[4] = { 0, 0, 1, 1 }, prevDraw = 0, prevRead = 0;
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	GLfloat clearDepth = 1.0f;
	fn->glGetFloatv( GL_DEPTH_CLEAR_VALUE, &clearDepth );
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	const GLenum att = prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
		GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
	GLenum fmt = 0;
	if ( depthBits == 24 && compType != GL_FLOAT )
		fmt = stencilBits == 8 ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
	else if ( depthBits == 32 && compType == GL_FLOAT )
		fmt = stencilBits == 8 ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32F;
	else if ( depthBits == 16 )
		fmt = GL_DEPTH_COMPONENT16;
	if ( !fmt ) {
		say( QStringLiteral( "far lod: sky census UNREADABLE (depth %1 bits, stencil %2)" ).arg( depthBits ).arg( stencilBits ) );
		return;
	}
	const int Wd = vp[0] + vp[2], Ht = vp[1] + vp[3];
	GLuint fbo = 0, rb = 0;
	fn->glGenFramebuffers( 1, &fbo );
	fn->glGenRenderbuffers( 1, &rb );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, rb );
	fn->glRenderbufferStorage( GL_RENDERBUFFER, fmt, Wd, Ht );
	fn->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );
	fn->glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER,
		stencilBits == 8 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb );
	fn->glDrawBuffer( GL_NONE );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBlitFramebuffer( 0, 0, Wd, Ht, 0, 0, Wd, Ht, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	fn->glReadBuffer( GL_NONE );
	std::vector<float> depth( size_t( vp[2] ) * size_t( vp[3] ), 0.0f );
	fn->glReadPixels( vp[0], vp[1], vp[2], vp[3], GL_DEPTH_COMPONENT, GL_FLOAT, depth.data() );
	const GLenum err = fn->glGetError();
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glDeleteRenderbuffers( 1, &rb );
	fn->glDeleteFramebuffers( 1, &fbo );
	if ( err != GL_NO_ERROR ) {
		say( QStringLiteral( "far lod: sky census UNREADABLE (GL error 0x%1)" ).arg( err, 0, 16 ) );
		return;
	}

	// a pixel is below the horizon when its view ray points down in the world (more than 1 degree)
	const Matrix toWorld = scene->view.rotation.inverted();
	/* FINALFIX: WHERE an empty ray goes. A ray that reaches sea level (z 0) past the far field's footprint passed
	 * over all of it: the footprint is the last ring's square, which holds the game's fBlockMaximumDistance
	 * (250000) circle, so the game draws no LOD there either -- sky, not a hole. One that reaches z 0 inside the
	 * footprint crossed where ground should be and is a hole. Counted apart so the gate can tell them (the far
	 * field off has no footprint: every pixel counts). Measured 10-06: the over view's 2058 (FO4CS) and 2001
	 * (Vanilla) empty pixels all land at cells x 7.5..104, y -140.9..-62.9: past the footprint (x to 63, y from -64). */
	const FarDoc * fdoc = farFor( scene );
	const float vs = scene->view.scale != 0.0f ? scene->view.scale : 1.0f;
	const Vector3 eye = toWorld * ( Vector3( 0, 0, 0 ) - scene->view.translation ) / vs;
	const bool haveFoot = fdoc && fdoc->fx1 >= fdoc->fx0 && fdoc->fy1 >= fdoc->fy0;
	const float kC = 4096.0f;
	qint64 pastEdge = 0;
	QStringList inside;
	float px0 = 1e30f, py0 = 1e30f, px1 = -1e30f, py1 = -1e30f;
	const float down = -std::sin( 3.14159265f / 180.0f );
	qint64 below = 0, empty = 0, emptyAbove = 0;
	int bx0 = vp[2], by0 = vp[3], bx1 = -1, by1 = -1;	// where the empty below-horizon pixels are (top-left origin)
	const QByteArray maskPath = qgetenv( "WW_CELL_FARLOD_SKYMASK" );
	std::vector<unsigned char> mask( maskPath.isEmpty() ? 0 : size_t( vp[2] ) * size_t( vp[3] ), 0 );
	for ( int y = 0; y < vp[3]; y++ ) {
		const float ndcY = ( float( y ) + 0.5f ) / float( vp[3] ) * 2.0f - 1.0f;
		for ( int x = 0; x < vp[2]; x++ ) {
			const float ndcX = ( float( x ) + 0.5f ) / float( vp[2] ) * 2.0f - 1.0f;
			const Vector3 d = toWorld * Vector3( ndcX / P00, ndcY / P11, -1.0f );
			const float len = d.length();
			const bool isEmpty = depth[size_t( y ) * size_t( vp[2] ) + size_t( x )] >= clearDepth;
			if ( len > 0.0f && d[2] / len < down ) {
				below++;
				if ( isEmpty ) {
					empty++;
					if ( haveFoot && d[2] < 0.0f ) {
						const float tz = -eye[2] / d[2];
						const float hx = eye[0] + d[0] * tz, hy = eye[1] + d[1] * tz;
						if ( tz > 0.0f && ( hx < float( fdoc->fx0 ) * kC || hx >= float( fdoc->fx1 + 1 ) * kC
								|| hy < float( fdoc->fy0 ) * kC || hy >= float( fdoc->fy1 + 1 ) * kC ) ) {
							pastEdge++;
							px0 = std::min( px0, hx / kC ); px1 = std::max( px1, hx / kC );
							py0 = std::min( py0, hy / kC ); py1 = std::max( py1, hy / kC );
						} else if ( tz > 0.0f && inside.size() < 6 ) {	// a hole: where (sea-level cells, to locate it)
							/* FINALFIX: and where the ground AROUND it is -- the nearest drawn pixel within 3,
							 * unprojected (world cells x, y and z units): a hole's own ray meets nothing. */
							QString around;
							const float P22 = r->globalUniforms->projectionMatrix[2][2];
							const float P32 = r->globalUniforms->projectionMatrix[3][2];
							for ( int rad = 1; rad <= 3 && around.isEmpty(); rad++ )
								for ( int oy = -rad; oy <= rad && around.isEmpty(); oy++ )
									for ( int ox = -rad; ox <= rad && around.isEmpty(); ox++ ) {
										const int qx = x + ox, qy = y + oy;
										if ( qx < 0 || qy < 0 || qx >= vp[2] || qy >= vp[3] )
											continue;
										const float dd = depth[size_t( qy ) * size_t( vp[2] ) + size_t( qx )];
										if ( dd >= clearDepth )
											continue;
										const float ze = -P32 / ( 2.0f * dd - 1.0f + P22 );
										const float qnx = ( float( qx ) + 0.5f ) / float( vp[2] ) * 2.0f - 1.0f;
										const float qny = ( float( qy ) + 0.5f ) / float( vp[3] ) * 2.0f - 1.0f;
										const Vector3 w = eye + toWorld * ( Vector3( qnx / P00, qny / P11, -1.0f ) * -ze ) / vs;
										around = QStringLiteral( " by ground at %1,%2 z %3" ).arg( double( w[0] / kC ), 0, 'f', 4 )
											.arg( double( w[1] / kC ), 0, 'f', 4 ).arg( double( w[2] ), 0, 'f', 1 );
									}
							inside << QStringLiteral( "(%1,%2%3)" ).arg( double( hx / kC ), 0, 'f', 3 ).arg( double( hy / kC ), 0, 'f', 3 ).arg( around );
						}
					}
					const int ty = vp[3] - 1 - y;
					bx0 = std::min( bx0, x ); bx1 = std::max( bx1, x );
					by0 = std::min( by0, ty ); by1 = std::max( by1, ty );
					if ( !mask.empty() )
						mask[size_t( ty ) * size_t( vp[2] ) + size_t( x )] = 255;
				}
			} else if ( isEmpty ) {
				emptyAbove++;
			}
		}
	}
	const FarDoc * f = farFor( scene );
	if ( !mask.empty() ) {	// a harness picture of WHERE: white = empty below the horizon (binary PGM)
		// whole or not at all: the shot's exit can land mid-write, so a temp file renamed over the last one
		const QString maskFile = QString::fromLocal8Bit( maskPath ), tmp = maskFile + QStringLiteral( ".part" );
		QFile mf( tmp );
		if ( mf.open( QIODevice::WriteOnly ) ) {
			mf.write( QStringLiteral( "P5 %1 %2 255\n" ).arg( vp[2] ).arg( vp[3] ).toLatin1() );
			mf.write( reinterpret_cast<const char *>( mask.data() ), qint64( mask.size() ) );
			if ( mf.flush() ) {
				mf.close();
				QFile::remove( maskFile );
				QFile::rename( tmp, maskFile );
			}
		}
	}
	say( QStringLiteral( "far lod: sky census %1 empty below-horizon pixels of %2 below (%3 empty above), far field %4%5" )
		.arg( empty ).arg( below ).arg( emptyAbove ).arg( f ? QStringLiteral( "ON" ) : QStringLiteral( "off" ) )
		.arg( empty ? QStringLiteral( "; empty box x %1..%2 y %3..%4 of %5x%6" ).arg( bx0 ).arg( bx1 ).arg( by0 ).arg( by1 )
				.arg( vp[2] ).arg( vp[3] ) : QString() )
		+ ( haveFoot ? QStringLiteral( "; %1 of them meet sea level past the far field's edge (footprint cells %2,%3..%4,%5%6)" )
				.arg( pastEdge ).arg( fdoc->fx0 ).arg( fdoc->fy0 ).arg( fdoc->fx1 ).arg( fdoc->fy1 )
				.arg( pastEdge ? QStringLiteral( "; they land at cells x %1..%2 y %3..%4" ).arg( double( px0 ), 0, 'f', 1 )
					.arg( double( px1 ), 0, 'f', 1 ).arg( double( py0 ), 0, 'f', 1 ).arg( double( py1 ), 0, 'f', 1 ) : QString() )
				+ ( inside.isEmpty() ? QString() : QStringLiteral( "; inside at cells %1" ).arg( inside.join( QLatin1Char( ' ' ) ) ) )
			: QString() ) );
}
