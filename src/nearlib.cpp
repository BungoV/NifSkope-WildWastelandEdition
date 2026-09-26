/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "nearlib.h"

#include "esmdata.h"
#include "lodgen.h"
#include "lodgenparallel.h"
#include "lodifile.h"
#include "lodofile.h"
#include "nativeemit.h"
#include "data/niftypes.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <tuple>

/* Lane NEAR1, 2026-09-26. The design and every rule are in nearlib.h; the
 * format fields in docs/LODGEN_NATIVE_LODO_LODI.md (.lodo v7, .lodi v11). */

namespace
{

constexpr quint32 NEAR_FLAG_IS_MARKER = 0x00800000U;   // STAT record flag 23 (wbDefinitionsFO4)
constexpr int NEAR_MODEL_WORKER_CAP = 4;                // the far loader's measured ceiling (nativeemit.cpp)
constexpr int NEAR_STAGE_BATCH = 256;

QString nearFold( const QString & s )
{
	QString t = s.toLower();
	t.replace( QChar( '/' ), QChar( '\\' ) );
	return t;
}

QString nearFourcc( quint32 t )
{
	char c[5];
	std::memcpy( c, &t, 4 );
	c[4] = 0;
	for ( int i = 0; i < 4; i++ )
		if ( c[i] < 32 || c[i] > 126 )
			c[i] = '?';
	return QString::fromLatin1( c );
}

bool nearIs( quint32 t, const char * s )
{
	return std::memcmp( &t, s, 4 ) == 0;
}

quint64 nearFnv( quint64 h, const QString & s )
{
	const QByteArray b = s.toUtf8();
	return lodoFnv1a64( b.constData(), size_t( b.size() ), h );
}

template <typename T>
quint64 nearFnvV( quint64 h, const T & v )
{
	return lodoFnv1a64( &v, sizeof( v ), h );
}

//! The far field's swap precedence (nativeemit.cpp nativeEffectiveSwap), restated.
quint32 nearEffectiveSwap( const EsmWorld & world, quint32 xmsp, quint32 nameBase, quint32 partBase )
{
	if ( xmsp )
		return xmsp;
	if ( nameBase && world.lodBase( nameBase ).materialSwap )
		return world.lodBase( nameBase ).materialSwap;
	if ( partBase && partBase != nameBase && world.lodBase( partBase ).materialSwap )
		return world.lodBase( partBase ).materialSwap;
	return 0;
}

//! What pass A keeps of one source shape (no geometry).
struct NearShapeSum
{
	NearShapeFacts nf;
	QString matName, tex0;
	bool emits = false;
	float emitMult = 0.0f;
	quint32 verts = 0, tris = 0;
	float bmin[3] = { 0, 0, 0 }, bmax[3] = { 0, 0, 0 };
	QString reason;         //!< empty = drawn
	QString matKey;         //!< drawn shapes only
};

struct NearModel
{
	QString name;           //!< the mesh row's string: folded path, + `|mswp:<hex>` on a variant
	QString path;           //!< what the loader is given
	bool variant = false;
	quint32 swapForm = 0;
	LodgenMaterialSubst swap;
	bool loaded = false;
	int controllers = 0;
	std::vector<NearShapeSum> shapes;
	QSet<QString> swapKeys;     //!< every shape's material, lodgenMaterialSwapKey-folded
	int kept = 0;
	float radius = 0.0f;
	bool anyAlphaTest = false, anyEmit = false;
	quint16 meshId = LODO_NO_MESH;
	quint32 keptTris = 0;
};

struct NearCand
{
	int refIndex = -1;
	quint32 ref = 0;
	int part = -1;
	quint32 base = 0;       //!< the base whose model is drawn (a SCOL part's own base)
	quint32 swap = 0;       //!< effective MSWP, before the variant test
	float pos[3] = { 0, 0, 0 };
	float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float scale = 1.0f;
	bool disabled = false;
	QString modelKey;       //!< folded plain model
	QString reason;         //!< empty = eligible
	QString useKey;         //!< the model key drawn (plain or variant)
	quint32 useSwap = 0;    //!< 0 = the plain base row
};

struct NearRefRow
{
	quint32 form = 0;
	QString type;
	bool scol = false;
	QString reason;         //!< empty = eligible
};

QString nearMaterialKey( const NearShapeSum & s )
{
	const NearShapeFacts & f = s.nf;
	QString k = QString( "fam%1|at%2|2s%3|px%4|env%5|grey%6|vc%7|msn%8|em%9:%10|" )
		.arg( f.pbr ? 1 : 0 ).arg( f.alphaTest ? int( std::max<quint8>( 1, f.alphaRef ) ) : 0 )
		.arg( f.twoSided ? 1 : 0 ).arg( f.parallax ? 1 : 0 ).arg( f.envMap ? 1 : 0 )
		.arg( f.greyscale ? 1 : 0 ).arg( f.vertexColour ? 1 : 0 ).arg( f.modelSpaceNormals ? 1 : 0 )
		.arg( s.emits ? 1 : 0 ).arg( double( s.emits ? s.emitMult : 0.0f ) );
	k += nearFold( s.matName.isEmpty() ? s.tex0 : s.matName );
	for ( const QString & t : f.textures )
		k += QChar( '|' ) + nearFold( t );
	return k;
}

QString nearShapeReason( const NearShapeFacts & f )
{
	if ( f.effectShader )
		return QStringLiteral( "effect" );
	if ( f.alphaBlend )
		return QStringLiteral( "alpha-blend" );
	if ( f.decal )
		return QStringLiteral( "decal" );
	if ( f.treeAnim )
		return QStringLiteral( "tree-anim" );
	return QString();
}

QString nearBucketOf( const LodoMaterial & m )
{
	if ( m.features & LODO_MAT_FEAT_PARALLAX )
		return QStringLiteral( "parallax" );
	const bool at = m.alphaThreshold != 0, two = ( m.flags & LODO_MAT_TWO_SIDED ) != 0;
	if ( at )
		return two ? QStringLiteral( "alpha-test-two-sided" ) : QStringLiteral( "alpha-test" );
	return two ? QStringLiteral( "two-sided" ) : QStringLiteral( "opaque" );
}

QString nearCounts( const QMap<QString, quint64> & m )
{
	QStringList parts;
	for ( auto it = m.constBegin(); it != m.constEnd(); ++it )
		parts << QString( "%1 %2" ).arg( it.key() ).arg( it.value() );
	return parts.isEmpty() ? QStringLiteral( "none" ) : parts.join( QStringLiteral( ", " ) );
}

bool nearWriteText( const QString & path, const QStringList & lines, qint64 * bytes, QString * err )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		*err = QString( "cannot write %1" ).arg( path );
		return false;
	}
	QByteArray b = lines.join( QChar( '\n' ) ).toUtf8();
	b.append( '\n' );
	if ( f.write( b ) != b.size() ) {
		*err = QString( "short write to %1" ).arg( path );
		return false;
	}
	*bytes = b.size();
	return true;
}

} // namespace

bool nearLibraryBake( const EsmWorld & world, const NearLibraryOptions & opts, QStringList * report, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = QStringLiteral( "near: " ) + m;
		return false;
	};
	QElapsedTimer clock;
	clock.start();
	const QString ws = world.worldspaceEdid();
	if ( ws.isEmpty() )
		return fail( QStringLiteral( "the worldspace has no editor ID" ) );
	int x0, y0, x1, y1;
	world.cellBounds( x0, y0, x1, y1 );
	if ( opts.haveRegion ) {
		x0 = std::min( opts.region[0], opts.region[2] );
		x1 = std::max( opts.region[0], opts.region[2] );
		y0 = std::min( opts.region[1], opts.region[3] );
		y1 = std::max( opts.region[1], opts.region[3] );
	}

	/* ---- 1. the ESM walk: every REFR in the region, judged on the records ---- */
	std::vector<EsmRefr> refs;
	for ( int cy = y0; cy <= y1; cy++ )
		for ( int cx = x0; cx <= x1; cx++ )
			if ( world.hasCell( cx, cy ) )
				for ( const EsmRefr & r : world.refrs( cx, cy ) )
					refs.push_back( r );
	for ( const EsmRefr & r : world.persistentRefrsIn( float( x0 ) * 4096.0f, float( y0 ) * 4096.0f,
			float( x1 + 1 ) * 4096.0f, float( y1 + 1 ) * 4096.0f ) )
		refs.push_back( r );

	std::vector<NearRefRow> refRows( refs.size() );
	std::vector<NearCand> cands;
	QMap<QString, quint64> partExcluded;
	QSet<quint32> seenRef;
	for ( size_t i = 0; i < refs.size(); i++ ) {
		const EsmRefr & r = refs[i];
		NearRefRow & row = refRows[i];
		row.form = r.formID;
		if ( seenRef.contains( r.formID ) ) {
			row.reason = QStringLiteral( "duplicate-ref" );
			continue;
		}
		seenRef.insert( r.formID );
		if ( r.deleted ) {
			row.reason = QStringLiteral( "deleted" );
			continue;
		}
		const EsmLodBase & lb = world.lodBase( r.base );
		row.type = r.base && lb.type ? nearFourcc( lb.type ) : QString();
		if ( !r.base || !lb.type ) {
			row.reason = QStringLiteral( "no-base" );
			continue;
		}
		const bool isStat = nearIs( lb.type, "STAT" ), isScol = nearIs( lb.type, "SCOL" );
		if ( !isStat && !isScol ) {
			row.reason = QStringLiteral( "type:" ) + nearFourcc( lb.type );
			continue;
		}
		if ( isStat && ( lb.recordFlags & NEAR_FLAG_IS_MARKER ) ) {
			row.reason = QStringLiteral( "marker" );
			continue;
		}
		if ( lb.hasDestructible ) {
			row.reason = QStringLiteral( "destructible" );
			continue;
		}
		Matrix rm;
		rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
		const Vector3 rp( r.pos[0], r.pos[1], r.pos[2] );
		auto make = [&]( quint32 base, int part, const Vector3 & p, const Matrix & m, float sc, quint32 swap,
				const QString & model ) {
			NearCand c;
			c.refIndex = int( i );
			c.ref = r.formID;
			c.part = part;
			c.base = base;
			c.swap = swap;
			for ( int k = 0; k < 3; k++ )
				c.pos[k] = p[k];
			for ( int a = 0; a < 3; a++ )
				for ( int b = 0; b < 3; b++ )
					c.rot[a * 3 + b] = m( a, b );
			c.scale = sc;
			c.disabled = r.initiallyDisabled;
			c.modelKey = nearFold( model );
			cands.push_back( c );
		};
		if ( isStat ) {
			if ( lb.model.isEmpty() ) {
				row.reason = QStringLiteral( "no-model" );
				continue;
			}
			make( r.base, -1, rp, rm, r.scale, nearEffectiveSwap( world, r.materialSwap, r.base, 0 ), lb.model );
			continue;
		}
		row.scol = true;
		const QVector<EsmScolPart> & parts = world.scolParts( r.base );
		if ( parts.isEmpty() ) {
			row.reason = QStringLiteral( "scol-no-parts" );
			continue;
		}
		int scolPart = 0;       // the far field's ordinal: every placement of every part
		for ( const EsmScolPart & part : parts ) {
			const EsmLodBase & pb = world.lodBase( part.base );
			QString pr;
			if ( !part.base || !pb.type )
				pr = QStringLiteral( "part-no-base" );
			else if ( !nearIs( pb.type, "STAT" ) )
				pr = QStringLiteral( "part-type:" ) + nearFourcc( pb.type );
			else if ( pb.recordFlags & NEAR_FLAG_IS_MARKER )
				pr = QStringLiteral( "part-marker" );
			else if ( pb.hasDestructible )
				pr = QStringLiteral( "part-destructible" );
			else if ( pb.model.isEmpty() )
				pr = QStringLiteral( "part-no-model" );
			for ( const EsmScolPlacement & pl : part.placements ) {
				const int ordinal = scolPart++;
				if ( !pr.isEmpty() ) {
					partExcluded[pr]++;
					continue;
				}
				Matrix pm;
				pm.fromEuler( -pl.rot[0], -pl.rot[1], -pl.rot[2] );
				const Vector3 pos = rp + rm * ( Vector3( pl.pos[0], pl.pos[1], pl.pos[2] ) * r.scale );
				make( part.base, ordinal, pos, rm * pm, r.scale * pl.scale,
					nearEffectiveSwap( world, r.materialSwap, r.base, part.base ), pb.model );
			}
		}
	}
	const qint64 msWalk = clock.elapsed();

	/* ---- 2. pass A: every distinct plain model, loaded once, facts kept ---- */
	void * user = const_cast<QString *>( &opts.dataRoot );
	std::map<QString, NearModel> models;       // key = mesh name; std::map keeps the name order
	for ( const NearCand & c : cands ) {
		NearModel & m = models[c.modelKey];
		m.name = c.modelKey;
		m.path = c.modelKey;
	}
	auto passA = [&]( NearModel & m ) {
		std::vector<NativeSrcShape> shapes;
		m.loaded = lodgenNativeLoadModelOnce( user, m.path, m.variant ? &m.swap : nullptr, &shapes ) && !shapes.empty();
		if ( !m.loaded )
			return;
		m.controllers = shapes.front().nearFacts.modelControllers;
		for ( const NativeSrcShape & sh : shapes ) {
			NearShapeSum s;
			s.nf = sh.nearFacts;
			s.matName = sh.matName;
			s.tex0 = sh.tex0;
			s.emits = sh.ownEmit && ( sh.emitColor[0] > 0.0f || sh.emitColor[1] > 0.0f || sh.emitColor[2] > 0.0f );
			s.emitMult = sh.emitMult;
			s.verts = quint32( sh.geom.pos.size() / 3 );
			s.tris = quint32( sh.geom.tris.size() / 3 );
			for ( int k = 0; k < 3; k++ ) {
				s.bmin[k] = 3.4e38f;
				s.bmax[k] = -3.4e38f;
			}
			for ( size_t v = 0; v + 2 < sh.geom.pos.size(); v += 3 ) {
				for ( int k = 0; k < 3; k++ ) {
					s.bmin[k] = std::min( s.bmin[k], sh.geom.pos[v + size_t( k )] );
					s.bmax[k] = std::max( s.bmax[k], sh.geom.pos[v + size_t( k )] );
				}
			}
			if ( !sh.matName.isEmpty() )
				m.swapKeys.insert( lodgenMaterialSwapKey( sh.matName ) );
			s.reason = nearShapeReason( s.nf );
			if ( s.reason.isEmpty() ) {
				s.matKey = nearMaterialKey( s );
				m.kept++;
				m.keptTris += s.tris;
				m.anyAlphaTest = m.anyAlphaTest || s.nf.alphaTest;
				m.anyEmit = m.anyEmit || s.emits;
				for ( size_t v = 0; v + 2 < sh.geom.pos.size(); v += 3 ) {
					const float * p = &sh.geom.pos[v];
					m.radius = std::max( m.radius, std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] ) );
				}
			}
			m.shapes.push_back( std::move( s ) );
		}
	};
	{
		std::vector<NearModel *> jobs;
		for ( auto & kv : models )
			jobs.push_back( &kv.second );
		lodgenParallelFor( int( jobs.size() ), [&]( int i ) { passA( *jobs[size_t( i )] ); }, NEAR_MODEL_WORKER_CAP );
	}
	const qint64 msPassA = clock.elapsed();

	/* ---- 3. placements judged on their models; SWAP1's variant rule ---- */
	auto plainOk = [&]( const NearModel & m, QString * why ) {
		if ( !m.loaded ) { *why = QStringLiteral( "model-unloadable" ); return false; }
		if ( m.controllers > 0 ) { *why = QStringLiteral( "animated" ); return false; }
		if ( m.kept == 0 ) { *why = QStringLiteral( "no-drawable-shape" ); return false; }
		return true;
	};
	std::set<std::pair<quint32, QString>> swapPairs;        // (MSWP, plain model key), ascending
	for ( NearCand & c : cands ) {
		const NearModel & m = models[c.modelKey];
		if ( !plainOk( m, &c.reason ) )
			continue;
		c.useKey = c.modelKey;
		if ( c.swap )
			swapPairs.insert( std::make_pair( c.swap, c.modelKey ) );
	}
	std::map<std::pair<quint32, QString>, QString> variantOf;   // (MSWP, plain key) -> variant key
	quint64 swapMissing = 0;
	{
		QHash<QString, QString> bySig;
		std::vector<QString> fresh;
		for ( const auto & pr : swapPairs ) {
			const quint32 w = pr.first;
			const QString & pk = pr.second;
			const EsmMaterialSwap & mw = world.materialSwap( w );
			if ( !mw.exists ) {
				swapMissing++;
				continue;
			}
			QMap<QString, QString> subst;
			for ( const EsmMaterialSubst & row : mw.rows ) {
				const QString k = lodgenMaterialSwapKey( row.original );
				if ( k.isEmpty() || row.replacement.isEmpty() || subst.contains( k ) )
					continue;
				subst.insert( k, row.replacement );
			}
			const NearModel & pm = models[pk];
			LodgenMaterialSubst hits;
			QString sig;
			for ( auto it = subst.constBegin(); it != subst.constEnd(); ++it ) {
				if ( !pm.swapKeys.contains( it.key() ) )
					continue;
				const QString rk = lodgenMaterialSwapKey( it.value() );
				if ( rk == it.key() )
					continue;
				hits.append( qMakePair( it.key(), it.value() ) );
				sig += it.key() + QChar( '>' ) + rk + QChar( '\n' );
			}
			if ( hits.isEmpty() )
				continue;
			const QString sigKey = pk + QChar( '\n' ) + sig;
			auto vb = bySig.constFind( sigKey );
			if ( vb != bySig.constEnd() ) {
				variantOf[pr] = vb.value();
				continue;
			}
			const QString vk = pk + QString( "|mswp:%1" ).arg( w, 8, 16, QChar( '0' ) );
			NearModel vm;
			vm.name = vk;
			vm.path = pk;
			vm.variant = true;
			vm.swapForm = w;
			vm.swap = hits;
			models[vk] = vm;
			bySig.insert( sigKey, vk );
			variantOf[pr] = vk;
			fresh.push_back( vk );
		}
		std::vector<NearModel *> jobs;
		for ( const QString & vk : fresh )
			jobs.push_back( &models[vk] );
		lodgenParallelFor( int( jobs.size() ), [&]( int i ) { passA( *jobs[size_t( i )] ); }, NEAR_MODEL_WORKER_CAP );
	}
	quint64 variantFallback = 0;
	for ( NearCand & c : cands ) {
		if ( !c.reason.isEmpty() || !c.swap )
			continue;
		const auto v = variantOf.find( std::make_pair( c.swap, c.modelKey ) );
		if ( v == variantOf.end() )
			continue;           // the swap names no material of this model: the plain row serves
		QString why;
		if ( !plainOk( models[v->second], &why ) ) {
			variantFallback++;  // the variant did not load or kept nothing: drawn plain, counted
			continue;
		}
		c.useKey = v->second;
		c.useSwap = c.swap;
	}
	for ( NearCand & c : cands )
		if ( c.reason.isEmpty() && !( c.scale >= 0.0f && c.scale <= LODI_SCALE_MAX_WIDE ) )
			c.reason = QStringLiteral( "scale-out-of-range" );

	/* ---- 4. the textures, bucketed by (DXGI, width, height) ---- */
	struct TexInfo { bool ok = false; quint32 dxgi = 0, w = 0, h = 0, mips = 0; QString src; };
	QMap<QString, TexInfo> texInfo;             // folded path -> header facts
	std::set<QString> usedModels;
	for ( const NearCand & c : cands )
		if ( c.reason.isEmpty() ) {
			usedModels.insert( c.useKey );
			usedModels.insert( c.modelKey );    // a variant row needs its plain row before it
		}
	for ( const QString & mk : usedModels )
		for ( const NearShapeSum & s : models[mk].shapes )
			if ( s.reason.isEmpty() )
				for ( const QString & t : s.nf.textures )
					if ( !t.isEmpty() )
						texInfo.insert( nearFold( t ), TexInfo() );
	for ( auto it = texInfo.begin(); it != texInfo.end(); ++it ) {
		TexInfo & ti = it.value();
		ti.ok = lodgenTextureInfo( opts.dataRoot, it.key(), &ti.dxgi, &ti.w, &ti.h, &ti.mips, &ti.src );
	}
	const qint64 msTextures = clock.elapsed();

	/* ---- 5. the material table ---- */
	struct MatRec { LodoMaterial row; QString lodm; QStringList textures; QString key; };
	QMap<QString, MatRec> matByKey;
	for ( const QString & mk : usedModels )
		for ( const NearShapeSum & s : models[mk].shapes ) {
			if ( !s.reason.isEmpty() || matByKey.contains( s.matKey ) )
				continue;
			MatRec r;
			std::memset( &r.row, 0, sizeof( r.row ) );
			r.row.arrayClass = 0;
			r.row.arraySet = 0;
			r.row.layer = LODO_NO_LAYER;
			r.row.family = s.nf.pbr ? LODO_FAMILY_PBR : LODO_FAMILY_LEGACY;
			r.row.alphaThreshold = s.nf.alphaTest ? std::max<quint8>( 1, s.nf.alphaRef ) : 0;
			r.row.flags = quint8( ( s.nf.twoSided ? LODO_MAT_TWO_SIDED : 0 ) | ( s.emits ? LODO_MAT_EMITS : 0 ) );
			r.row.features = quint8( ( s.nf.parallax ? LODO_MAT_FEAT_PARALLAX : 0 ) | ( s.nf.envMap ? LODO_MAT_FEAT_ENV_MAP : 0 )
				| ( s.nf.greyscale ? LODO_MAT_FEAT_GREYSCALE : 0 ) | ( s.nf.vertexColour ? LODO_MAT_FEAT_VERTEX_COLOUR : 0 )
				| ( s.nf.modelSpaceNormals ? LODO_MAT_FEAT_MODEL_SPACE_NORMALS : 0 ) );
			r.row.emissiveScale = s.emits ? s.emitMult : 0.0f;
			r.lodm = s.matName.isEmpty() ? s.tex0 : s.matName;
			r.textures = s.nf.textures;
			r.key = s.matKey;
			matByKey.insert( s.matKey, r );
		}
	// diffuse buckets: (dxgi, w, h) -> the sorted distinct diffuse paths; split every 2048 layers
	std::map<std::tuple<quint32, quint32, quint32>, std::set<QString>> diffuseBuckets;
	for ( const MatRec & r : matByKey ) {
		const QString d = r.textures.isEmpty() ? QString() : nearFold( r.textures.front() );
		if ( d.isEmpty() || !texInfo.value( d ).ok )
			continue;
		const TexInfo & ti = texInfo[d];
		diffuseBuckets[std::make_tuple( ti.dxgi, ti.w, ti.h )].insert( d );
	}
	struct SetRow { quint32 dxgi, w, h, part, layers; };
	std::vector<SetRow> sets;
	QHash<QString, QPair<int, int>> diffuseLayer;     // folded diffuse -> (set, layer)
	quint64 layerlessSets = 0;
	for ( const auto & b : diffuseBuckets ) {
		int layer = 0;
		int set = -1;
		quint32 part = 0;
		for ( const QString & d : b.second ) {
			if ( set < 0 || layer >= int( LODO_LAYER_CAP ) ) {
				if ( set >= 0 )
					part++;
				sets.push_back( SetRow{ std::get<0>( b.first ), std::get<1>( b.first ), std::get<2>( b.first ), part, 0 } );
				set = int( sets.size() ) - 1;
				layer = 0;
			}
			diffuseLayer.insert( d, qMakePair( set, layer ) );
			sets[size_t( set )].layers++;
			layer++;
		}
	}
	for ( MatRec & r : matByKey ) {
		const QString d = r.textures.isEmpty() ? QString() : nearFold( r.textures.front() );
		const auto dl = diffuseLayer.constFind( d );
		if ( dl == diffuseLayer.constEnd() )
			continue;
		if ( dl.value().first > 255 ) {
			layerlessSets++;        // past the u8 arraySet: stays NO_LAYER, counted
			continue;
		}
		r.row.arraySet = quint8( dl.value().first );
		r.row.layer = quint16( dl.value().second );
	}
	std::vector<const MatRec *> matOrder;
	for ( const MatRec & r : matByKey )
		matOrder.push_back( &r );
	std::sort( matOrder.begin(), matOrder.end(), []( const MatRec * a, const MatRec * b ) {
		return std::make_tuple( a->row.family, a->row.arrayClass, a->row.arraySet, a->row.layer, nearFold( a->lodm ), a->key )
			< std::make_tuple( b->row.family, b->row.arrayClass, b->row.arraySet, b->row.layer, nearFold( b->lodm ), b->key );
	} );
	if ( matOrder.size() > 0xFFFF )
		return fail( QString( "%1 materials; the u16 material id holds 65,535" ).arg( matOrder.size() ) );

	LodoLibrary lib;
	lib.flags = LODO_FLAG_VERTEX_V1 | LODO_FLAG_CACHE_ORDER | LODO_FLAG_NEAR;
	lib.worldspaceEdid = ws;
	QHash<QString, quint16> materialId;
	for ( size_t i = 0; i < matOrder.size(); i++ ) {
		LodoMaterial row = matOrder[i]->row;
		row.lodmStringOffset = lib.addString( matOrder[i]->lodm );
		lib.materials.push_back( row );
		materialId.insert( matOrder[i]->key, quint16( i ) );
	}

	/* ---- 6. pass B: reload the used models, stage and merge in name order ---- */
	std::vector<NearModel *> meshJobs;
	for ( auto & kv : models )
		if ( usedModels.count( kv.first ) )
			meshJobs.push_back( &kv.second );
	if ( meshJobs.size() > size_t( LODO_NO_MESH ) )
		return fail( QString( "%1 meshes; the u16 mesh id holds 65,535" ).arg( meshJobs.size() ) );
	quint64 passBMismatch = 0;
	QString passBFirst;
	{
		LodoLibrary proto;
		proto.flags = lib.flags;
		proto.ladderFoliage = lib.ladderFoliage;
		proto.silhouetteMin = lib.silhouetteMin;
		proto.materials = lib.materials;
		proto.worldspaceEdid = lib.worldspaceEdid;
		struct Job { LodoLibrary staged; bool ok = false; QString err; LodoMeshStats ms; };
		for ( size_t first = 0; first < meshJobs.size(); first += NEAR_STAGE_BATCH ) {
			const size_t count = std::min<size_t>( NEAR_STAGE_BATCH, meshJobs.size() - first );
			std::vector<Job> jobs( count );
			lodgenParallelFor( int( count ), [&]( int i ) {
				NearModel & m = *meshJobs[first + size_t( i )];
				Job & j = jobs[size_t( i )];
				std::vector<NativeSrcShape> shapes;
				if ( !lodgenNativeLoadModelOnce( user, m.path, m.variant ? &m.swap : nullptr, &shapes )
						|| shapes.size() != m.shapes.size() ) {
					j.err = QString( "%1: pass B loaded %2 shapes, pass A %3" ).arg( m.name ).arg( shapes.size() ).arg( m.shapes.size() );
					return;
				}
				std::vector<LodoSrcShape> src;
				for ( size_t k = 0; k < shapes.size(); k++ ) {
					const NearShapeSum & s = m.shapes[k];
					if ( !s.reason.isEmpty() )
						continue;
					if ( quint32( shapes[k].geom.tris.size() / 3 ) != s.tris ) {
						j.err = QString( "%1 shape %2: pass B %3 triangles, pass A %4" ).arg( m.name ).arg( k )
							.arg( shapes[k].geom.tris.size() / 3 ).arg( s.tris );
						return;
					}
					LodoSrcShape g = std::move( shapes[k].geom );
					g.materialId = materialId.value( s.matKey );
					g.sway.clear();
					g.ao.assign( g.pos.size() / 3, 255 );   // self-AO not cast on full detail: 255 = not baked
					src.push_back( std::move( g ) );
				}
				j.ok = lodoStageMesh( j.staged, proto, src, m.name, &j.err, &j.ms );
			}, NEAR_MODEL_WORKER_CAP );
			for ( size_t i = 0; i < count; i++ ) {
				NearModel & m = *meshJobs[first + i];
				Job & j = jobs[i];
				if ( !j.ok ) {
					passBMismatch++;
					if ( passBFirst.isEmpty() )
						passBFirst = j.err;
					continue;
				}
				QString mergeErr;
				if ( !lodoMergeStagedMesh( lib, j.staged, m.name, &m.meshId, &mergeErr ) )
					return fail( mergeErr );
			}
		}
	}
	if ( passBMismatch )
		return fail( QString( "%1 model(s) could not be staged; first: %2" ).arg( passBMismatch ).arg( passBFirst ) );
	const qint64 msMeshes = clock.elapsed();

	/* ---- 7. base rows, (formId, materialSwap) ascending, plain before variant ---- */
	std::map<std::pair<quint32, quint32>, QString> baseModel;      // (base, swap) -> model key
	for ( const NearCand & c : cands )
		if ( c.reason.isEmpty() ) {
			baseModel[std::make_pair( c.base, quint32( 0 ) )] = c.modelKey;
			if ( c.useSwap )
				baseModel[std::make_pair( c.base, c.useSwap )] = c.useKey;
		}
	if ( baseModel.size() > size_t( LODI_BASE_MAX ) + 1 )
		return fail( QString( "%1 base rows; the u16 baseId holds 65,536" ).arg( baseModel.size() ) );
	std::map<std::pair<quint32, quint32>, quint32> baseRow;
	for ( const auto & bm : baseModel ) {
		const NearModel & m = models[bm.second];
		const LodoMesh & mesh = lib.meshes[m.meshId];
		LodoBase row;
		std::memset( &row, 0, sizeof( row ) );
		row.formId = bm.first.first;
		row.modelStringOffset = lib.addString( world.lodBase( bm.first.first ).model );
		row.rep[0] = m.meshId;
		row.rep[1] = row.rep[2] = row.rep[3] = LODO_NO_MESH;
		row.cardLayer = LODO_NO_CARD;
		row.flags = quint16( LODO_BASE_ANY_MESH | ( m.anyAlphaTest ? LODO_BASE_ANY_ALPHA : 0 )
			| ( bm.first.second ? LODO_BASE_SWAPPED : 0 ) );
		row.boundRadius = m.radius > 0.0f ? m.radius : 1.0e-3f;
		row.fullTriangles = 0;
		for ( quint32 c = mesh.clusterFirst; c < mesh.clusterFirst + mesh.clusterCount; c++ )
			if ( lib.clusterLods[c].level == 0 )
				row.fullTriangles += lib.clusters[c].triangleCount;
		row.materialSwap = bm.first.second;
		baseRow[bm.first] = quint32( lib.bases.size() );
		lib.bases.push_back( row );
	}
	if ( lib.bases.empty() )
		return fail( QStringLiteral( "no eligible placement in the region; nothing to write" ) );

	// the hashes: pure functions of what reached the files
	quint64 objHash = Q_UINT64_C( 0xCBF29CE484222325 ), modelHash = objHash;
	for ( const NearCand & c : cands ) {
		if ( !c.reason.isEmpty() )
			continue;
		objHash = nearFnvV( objHash, c.ref );
		objHash = nearFnvV( objHash, c.part );
		objHash = nearFnvV( objHash, c.base );
		objHash = nearFnvV( objHash, c.useSwap );
		objHash = lodoFnv1a64( c.pos, sizeof( c.pos ), objHash );
		objHash = lodoFnv1a64( c.rot, sizeof( c.rot ), objHash );
		objHash = nearFnvV( objHash, c.scale );
		objHash = nearFnvV( objHash, quint8( c.disabled ? 1 : 0 ) );
	}
	for ( const NearModel * m : meshJobs ) {
		modelHash = nearFnv( modelHash, m->name );
		modelHash = nearFnvV( modelHash, m->keptTris );
	}
	lib.pluginCorpusHash = world.vhgtCorpusHash();
	lib.objectCorpusHash = objHash;
	lib.modelCorpusHash = modelHash;
	lib.loadOrderHash = world.loadOrderHash();

	if ( !QDir().mkpath( opts.outDir ) )
		return fail( QString( "cannot create %1" ).arg( opts.outDir ) );
	const QString stem = QDir( opts.outDir ).filePath( ws + QStringLiteral( ".near" ) );
	LodoHeader lh;
	QString err;
	if ( !lodoWrite( stem + QStringLiteral( ".lodo" ), lib, &lh, &err ) )
		return fail( err );

	/* ---- 8. the instances ---- */
	std::vector<quint16> drawKey( lib.bases.size(), 0 );
	{
		std::vector<std::pair<quint16, quint16>> pairs( lib.bases.size() );
		for ( size_t i = 0; i < lib.bases.size(); i++ ) {
			const quint16 mid = lib.bases[i].rep[0];
			const LodoMesh & m = lib.meshes[mid];
			pairs[i] = std::make_pair( mid, m.clusterCount ? lib.clusters[m.clusterFirst].materialId : quint16( 0xFFFF ) );
		}
		std::vector<std::pair<quint16, quint16>> distinct = pairs;
		std::sort( distinct.begin(), distinct.end() );
		distinct.erase( std::unique( distinct.begin(), distinct.end() ), distinct.end() );
		if ( distinct.size() > 0xFFFF )
			return fail( QString( "%1 distinct draw pairs; the u16 drawKey holds 65,535" ).arg( distinct.size() ) );
		for ( size_t i = 0; i < pairs.size(); i++ )
			drawKey[i] = quint16( std::lower_bound( distinct.begin(), distinct.end(), pairs[i] ) - distinct.begin() );
	}
	LodiSrcSet set;
	set.worldspaceEdid = ws;
	set.pluginCorpusHash = lib.pluginCorpusHash;
	set.objectCorpusHash = lib.objectCorpusHash;
	set.lodoIdentity = lodoIdentityOf( lh.headerCrc32, lh.modelCorpusHash, lh.objectCorpusHash );
	set.loadOrderHash = lib.loadOrderHash;
	set.placementAo = true;     // 0xFF bytes: never cast (the v7 header block needs the v5/v6 blobs present)
	set.vertexAo = true;
	set.group = true;           // every placement its own group
	quint64 nDisabled = 0, nScrap = 0, nParts = 0, nAlpha = 0, nEmit = 0;
	for ( const NearCand & c : cands ) {
		if ( !c.reason.isEmpty() )
			continue;
		const quint32 row = baseRow.at( std::make_pair( c.base, c.useSwap ) );
		const NearModel & m = models[c.useKey];
		LodiSrcInstance in;
		for ( int k = 0; k < 3; k++ )
			in.pos[k] = c.pos[k];
		for ( int k = 0; k < 9; k++ )
			in.rot[k] = c.rot[k];
		in.scale = c.scale;
		in.baseId = row;
		in.refFormId = c.ref;
		in.scolPart = qint16( c.part );
		in.drawKey = drawKey[row];
		in.boundRadius = lib.bases[row].boundRadius;
		in.baseName = world.lodBase( c.base ).edid;
		quint16 fl = 0;
		if ( c.part >= 0 ) { fl |= LODI_INST_SCOL_PART; nParts++; }
		if ( m.anyAlphaTest ) { fl |= LODI_INST_ALPHA_TESTED; nAlpha++; }
		if ( m.anyEmit ) { fl |= LODI_INST_EMITS; nEmit++; }
		if ( world.scrappable( c.base, c.pos ) ) { fl |= LODI_INST_SCRAPPABLE; nScrap++; }
		if ( c.disabled ) { fl |= LODI_INST_INITIALLY_DISABLED; nDisabled++; }
		in.flags = fl;
		set.instances.push_back( std::move( in ) );
	}
	LodiHeader ih;
	LodiWriteStats is;
	if ( !lodiWrite( stem + QStringLiteral( ".lodi" ), set, &ih, &is, &err ) )
		return fail( err );
	const qint64 msWrite = clock.elapsed();

	/* ---- 9. the sidecars ---- */
	QStringList tx;
	tx << QString( "# near texture sidecar v1, worldspace %1 (lane NEAR1). A material's arraySet/layer index a"
		" DIFFUSE set below; every slot's own (format, size) bucket is listed per material. Nothing re-encoded." ).arg( ws );
	tx << QStringLiteral( "# set <id> dxgi <code> w <px> h <px> part <n> layers <count>" );
	for ( size_t i = 0; i < sets.size(); i++ )
		tx << QString( "set %1 dxgi %2 w %3 h %4 part %5 layers %6" ).arg( i ).arg( sets[i].dxgi ).arg( sets[i].w )
			.arg( sets[i].h ).arg( sets[i].part ).arg( sets[i].layers );
	tx << QStringLiteral( "# layer <set> <layer> <diffuse path>" );
	{
		std::vector<std::tuple<int, int, QString>> ls;
		for ( auto it = diffuseLayer.constBegin(); it != diffuseLayer.constEnd(); ++it )
			ls.push_back( std::make_tuple( it.value().first, it.value().second, it.key() ) );
		std::sort( ls.begin(), ls.end() );
		for ( const auto & l : ls )
			tx << QString( "layer %1 %2 %3" ).arg( std::get<0>( l ) ).arg( std::get<1>( l ) ).arg( std::get<2>( l ) );
	}
	tx << QStringLiteral( "# tex <path> dxgi <code> w <px> h <px> mips <n> <ba2|loose|read>, or tex <path> missing" );
	QMap<QString, quint64> texBuckets;
	quint64 texMissing = 0;
	for ( auto it = texInfo.constBegin(); it != texInfo.constEnd(); ++it ) {
		const TexInfo & ti = it.value();
		if ( !ti.ok ) {
			tx << QString( "tex %1 missing" ).arg( it.key() );
			texMissing++;
			continue;
		}
		tx << QString( "tex %1 dxgi %2 w %3 h %4 mips %5 %6" ).arg( it.key() ).arg( ti.dxgi ).arg( ti.w ).arg( ti.h )
			.arg( ti.mips ).arg( ti.src );
		texBuckets[QString( "%1/%2x%3" ).arg( ti.dxgi ).arg( ti.w ).arg( ti.h )]++;
	}
	tx << QStringLiteral( "# mat <id> set <s> layer <l|none> bucket <name> family <legacy|pbr> lodm <path>, then slot <i> <path|-> <dxgi/wxh|missing|->" );
	QMap<QString, quint64> matBuckets;
	quint64 matPbr = 0;
	for ( size_t i = 0; i < matOrder.size(); i++ ) {
		const MatRec & r = *matOrder[i];
		const LodoMaterial & row = lib.materials[i];
		const QString bucket = nearBucketOf( row );
		matBuckets[bucket]++;
		if ( row.family == LODO_FAMILY_PBR )
			matPbr++;
		tx << QString( "mat %1 set %2 layer %3 bucket %4 family %5 lodm %6" ).arg( i ).arg( row.arraySet )
			.arg( row.layer == LODO_NO_LAYER ? QStringLiteral( "none" ) : QString::number( row.layer ) ).arg( bucket )
			.arg( row.family == LODO_FAMILY_PBR ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) ).arg( r.lodm );
		for ( int k = 0; k < r.textures.size(); k++ ) {
			const QString t = nearFold( r.textures[k] );
			QString b = QStringLiteral( "-" );
			if ( !t.isEmpty() ) {
				const TexInfo ti = texInfo.value( t );
				b = ti.ok ? QString( "%1/%2x%3" ).arg( ti.dxgi ).arg( ti.w ).arg( ti.h ) : QStringLiteral( "missing" );
			}
			tx << QString( "  slot %1 %2 %3" ).arg( k ).arg( t.isEmpty() ? QStringLiteral( "-" ) : t ).arg( b );
		}
	}
	qint64 txBytes = 0, shBytes = 0, rfBytes = 0;
	if ( !nearWriteText( stem + QStringLiteral( ".textures.txt" ), tx, &txBytes, &err ) )
		return fail( err );

	QStringList sh;
	sh << QStringLiteral( "# near shapes v1: mesh<TAB>path<TAB>variant<TAB>block<TAB>name<TAB>verts<TAB>tris<TAB>lod0"
		"<TAB>minx miny minz maxx maxy maxz<TAB>kept|reason<TAB>material|-<TAB>meshId|-" );
	QMap<QString, quint64> shapeExcluded;
	quint64 shapesKept = 0;
	std::set<std::pair<quint16, quint16>> meshMatPairs;
	for ( const auto & kv : models ) {
		const NearModel & m = kv.second;
		if ( !m.loaded ) {
			sh << QString( "%1\t%2\t%3\t-\t-\t-\t-\t-\t-\tmodel-unloadable\t-\t-" ).arg( m.name, m.path ).arg( m.variant ? 1 : 0 );
			continue;
		}
		const bool used = usedModels.count( kv.first ) != 0;
		for ( const NearShapeSum & s : m.shapes ) {
			if ( used ) {
				if ( s.reason.isEmpty() ) {
					shapesKept++;
					meshMatPairs.insert( std::make_pair( m.meshId, materialId.value( s.matKey ) ) );
				} else {
					shapeExcluded[s.reason]++;
				}
			}
			sh << QString( "%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9 %10 %11 %12 %13 %14\t%15\t%16\t%17" )
				.arg( m.name, m.path ).arg( m.variant ? 1 : 0 ).arg( s.nf.block ).arg( s.nf.name ).arg( s.verts ).arg( s.tris )
				.arg( s.nf.lod0Tris )
				.arg( double( s.bmin[0] ), 0, 'g', 9 ).arg( double( s.bmin[1] ), 0, 'g', 9 ).arg( double( s.bmin[2] ), 0, 'g', 9 )
				.arg( double( s.bmax[0] ), 0, 'g', 9 ).arg( double( s.bmax[1] ), 0, 'g', 9 ).arg( double( s.bmax[2] ), 0, 'g', 9 )
				.arg( s.reason.isEmpty() ? ( m.controllers ? QStringLiteral( "model-animated" ) : QStringLiteral( "kept" ) ) : s.reason )
				.arg( s.reason.isEmpty() && used ? QString::number( materialId.value( s.matKey ) ) : QStringLiteral( "-" ) )
				.arg( used ? QString::number( m.meshId ) : QStringLiteral( "-" ) );
		}
	}
	if ( !nearWriteText( stem + QStringLiteral( ".shapes.txt" ), sh, &shBytes, &err ) )
		return fail( err );

	// per REFR: its outcome; per placement: where it went
	QMap<QString, quint64> refExcluded, placeExcluded;
	quint64 refsEligible = 0, placements = 0;
	std::vector<int> eligibleParts( refs.size(), 0 );
	std::vector<QString> firstPartReason( refs.size() );
	for ( const NearCand & c : cands ) {
		if ( c.reason.isEmpty() )
			eligibleParts[size_t( c.refIndex )]++;
		else if ( firstPartReason[size_t( c.refIndex )].isEmpty() )
			firstPartReason[size_t( c.refIndex )] = c.reason;
	}
	for ( size_t i = 0; i < refs.size(); i++ ) {
		NearRefRow & row = refRows[i];
		if ( !row.reason.isEmpty() )
			continue;
		if ( eligibleParts[i] )
			continue;
		if ( row.scol )
			row.reason = QStringLiteral( "scol-no-eligible-part" );
		else
			row.reason = firstPartReason[i].isEmpty() ? QStringLiteral( "no-placement" ) : firstPartReason[i];
	}
	QStringList rf;
	rf << QStringLiteral( "# near refs v1: R<TAB>ref<TAB>type<TAB>eligible|reason; P<TAB>ref<TAB>part<TAB>base<TAB>swap"
		"<TAB>baseRow|-<TAB>eligible|reason<TAB>disabled" );
	for ( size_t i = 0; i < refs.size(); i++ ) {
		const NearRefRow & row = refRows[i];
		if ( row.reason.isEmpty() )
			refsEligible++;
		else
			refExcluded[row.reason]++;
		rf << QString( "R\t%1\t%2\t%3" ).arg( row.form, 8, 16, QChar( '0' ) ).arg( row.type.isEmpty() ? QStringLiteral( "-" ) : row.type )
			.arg( row.reason.isEmpty() ? QStringLiteral( "eligible" ) : row.reason );
	}
	for ( const NearCand & c : cands ) {
		if ( c.reason.isEmpty() )
			placements++;
		else if ( refRows[size_t( c.refIndex )].scol )
			placeExcluded[c.reason]++;
		const auto br = baseRow.find( std::make_pair( c.base, c.useSwap ) );
		rf << QString( "P\t%1\t%2\t%3\t%4\t%5\t%6\t%7" ).arg( c.ref, 8, 16, QChar( '0' ) ).arg( c.part )
			.arg( c.base, 8, 16, QChar( '0' ) ).arg( c.useSwap, 8, 16, QChar( '0' ) )
			.arg( c.reason.isEmpty() && br != baseRow.end() ? QString::number( br->second ) : QStringLiteral( "-" ) )
			.arg( c.reason.isEmpty() ? QStringLiteral( "eligible" ) : c.reason ).arg( c.disabled ? 1 : 0 );
	}
	if ( !nearWriteText( stem + QStringLiteral( ".refs.txt" ), rf, &rfBytes, &err ) )
		return fail( err );

	/* ---- 10. the census line ---- */
	quint64 tris = 0, variants = 0;
	for ( const LodoCluster & c : lib.clusters )
		tris += c.triangleCount;
	for ( const NearModel * m : meshJobs )
		if ( m->variant )
			variants++;
	quint64 excludedSum = 0;
	for ( quint64 v : refExcluded )
		excludedSum += v;
	QMap<QString, quint64> pairBuckets;
	for ( const auto & pr : meshMatPairs )
		pairBuckets[nearBucketOf( lib.materials[pr.second] )]++;
	const qint64 lodoBytes = QFileInfo( stem + QStringLiteral( ".lodo" ) ).size();
	const qint64 lodiBytes = QFileInfo( stem + QStringLiteral( ".lodi" ) ).size();
	if ( report ) {
		*report << QString( "near census: worldspace %1, cells x %2..%3 y %4..%5; refs read %6, eligible %7, excluded %8 "
			"(%9); sum %10" ).arg( ws ).arg( x0 ).arg( x1 ).arg( y0 ).arg( y1 ).arg( refs.size() ).arg( refsEligible )
			.arg( excludedSum ).arg( nearCounts( refExcluded ) )
			.arg( refsEligible + excludedSum == quint64( refs.size() ) ? QStringLiteral( "ok" ) : QStringLiteral( "MISMATCH" ) );
		*report << QString( "near placements: %1 (scol parts %2, initially disabled %3, scrappable %4, alpha-tested %5, "
			"emits %6); scol part placements excluded (%7); scol parts skipped by record (%8)" ).arg( placements )
			.arg( nParts ).arg( nDisabled ).arg( nScrap ).arg( nAlpha ).arg( nEmit ).arg( nearCounts( placeExcluded ) )
			.arg( nearCounts( partExcluded ) );
		*report << QString( "near shapes: kept %1, excluded (%2)" ).arg( shapesKept ).arg( nearCounts( shapeExcluded ) );
		*report << QString( "near library: meshes %1 (swap variants %2, swap pairs whose MSWP is missing %3, variants "
			"drawn plain %4), bases %5, materials %6 (%7; legacy %8, pbr %9)" ).arg( lib.meshes.size() ).arg( variants )
			.arg( swapMissing ).arg( variantFallback ).arg( lib.bases.size() ).arg( lib.materials.size() )
			.arg( nearCounts( matBuckets ) ).arg( quint64( lib.materials.size() ) - matPbr ).arg( matPbr );
		*report << QString( "near draw: distinct (mesh, material) pairs %1 (%2); triangles %3, clusters %4 (<= %5 triangles each), "
			"vertices %6" ).arg( meshMatPairs.size() ).arg( nearCounts( pairBuckets ) ).arg( tris ).arg( lib.clusters.size() )
			.arg( LODO_CLUSTER_MAX_TRIS ).arg( lib.vertices.size() );
		*report << QString( "near textures: %1 distinct, %2 missing, %3 (format/size) buckets, %4 diffuse sets (%5 past the "
			"u8 arraySet)" ).arg( texInfo.size() ).arg( texMissing ).arg( texBuckets.size() ).arg( sets.size() ).arg( layerlessSets );
		*report << QString( "near files: %1.lodo %2 bytes (v%3), %1.lodi %4 bytes (v%5), textures.txt %6, shapes.txt %7, "
			"refs.txt %8" ).arg( QFileInfo( stem ).fileName() ).arg( lodoBytes ).arg( lh.version ).arg( lodiBytes )
			.arg( is.version ).arg( txBytes ).arg( shBytes ).arg( rfBytes );
		*report << QString( "near times: walk %1 s, models %2 s, textures %3 s, meshes %4 s, write %5 s, total %6 s" )
			.arg( msWalk / 1000.0, 0, 'f', 1 ).arg( ( msPassA - msWalk ) / 1000.0, 0, 'f', 1 )
			.arg( ( msTextures - msPassA ) / 1000.0, 0, 'f', 1 ).arg( ( msMeshes - msTextures ) / 1000.0, 0, 'f', 1 )
			.arg( ( msWrite - msMeshes ) / 1000.0, 0, 'f', 1 ).arg( clock.elapsed() / 1000.0, 0, 'f', 1 );
	}
	return true;
}
